#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/ptrace.h>

#include <linux/kvm.h>

static const char *kvm_exit_reasons[] = {
	"KVM_EXIT_UNKNOWN",
	"KVM_EXIT_EXCEPTION",
	"KVM_EXIT_IO",
	"KVM_EXIT_HYPERCALL",
	"KVM_EXIT_DEBUG",
	"KVM_EXIT_HLT",
	"KVM_EXIT_MMIO",
	"KVM_EXIT_IRQ_WINDOW_OPEN",
	"KVM_EXIT_SHUTDOWN",
	"KVM_EXIT_FAIL_ENTRY",
	"KVM_EXIT_INTR",
	"KVM_EXIT_SET_TPR",
	"KVM_EXIT_TPR_ACCESS",
	"KVM_EXIT_S390_SIEIC",
	"KVM_EXIT_S390_RESET",
	"KVM_EXIT_DCR",
	"KVM_EXIT_NMI",
	"KVM_EXIT_INTERNAL_ERROR",
	"KVM_EXIT_OSI",
	"KVM_EXIT_PAPR_HCALL",
	"KVM_EXIT_S390_UCONTROL",
	"KVM_EXIT_WATCHDOG",
	"KVM_EXIT_S390_TSCH",
	"KVM_EXIT_EPR",
	"KVM_EXIT_SYSTEM_EVENT",
	"KVM_EXIT_S390_STSI",
	"KVM_EXIT_IOAPIC_EOI",
	"KVM_EXIT_HYPERV",
	"KVM_EXIT_ARM_NISV",
};

#define CONSOLE_FIFO_ADDR	0x01000000
#define MMIO_PING_CNTFRQ_ADDR	0x02000000
#define MMIO_PING_LAT_ADDR	0x02100000

#define MMIO_CNTFREQ_VAL	0x02000000
#define MMIO_PING_NOP		0x02000000
#define MMIO_PING_VAL		0x02100000

#if defined(__aarch64__)
#define GUEST_CODE_FILE "guest-aarch64.bin"
#elif defined(__x86_64__)
#define GUEST_CODE_FILE "guest-x86_64.bin"
#else
#error "arch not supported"
#endif

int main(int argc, char *argv[])
{
	struct kvm_userspace_memory_region region = {};
	struct kvm_run *vcpu_kvm_run;
	int kvmfd, vmfd, vcpufd, codefd;
	int ret, offset = 0;
	uint64_t cntfreq = 0;
	void *memory;

	codefd = open(GUEST_CODE_FILE, O_RDONLY);
	if (codefd < 0) {
		printf("Unable to open file %s", GUEST_CODE_FILE);
		return -EIO;
	}

	/* KVM interface */
	kvmfd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
	if (kvmfd < 0) {
		perror("Unable to open /dev/kvm");
		return -EINVAL;
	}

	/* Create a new virtual machine */
	vmfd = ioctl(kvmfd, KVM_CREATE_VM, (unsigned long)0);
	if (vmfd < 0) {
		perror("Unable to create VM");
		return -EIO;
	}

	/* Anonymous mapping to map an area of the process's virtual memory
	 * in page sized unit. "physical" address space as seen by the VM.
	 */
	memory = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE,
		      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (memory == NULL) {
		perror("Unable to mmap");
		return -ENOMEM;
	}

	/* Copy guest code into guest memory */
	while ((ret = read(codefd, memory + offset, 0x1000)) > 0)
		offset += ret;
	close(codefd);

	/* Tell KVM about this memory, mapped to VM physical address 0
	 * since vCPU PC is reset to 0 on init.
	 */
	region.slot = 0;
	region.guest_phys_addr = 0x0;
	region.memory_size = 0x1000,
	region.userspace_addr = (uint64_t)memory,

	ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
	if (ret < 0) {
		perror("Unable to set VM memory");
		return -EIO;
	}

	/* Create one vCPU (virtual CPU) */
	vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long)0);
	if (vcpufd < 0) {
		perror("Unable to create vCPU");
		return -EIO;
	}

	/* A kvm_run data structure is used to communicate information about
	 * the vCPU between the kernel and user space. Its size usually exceeds
	 * size of the kvm_run structure since the kernel will also use that
	 * space to store other transient structures. Size can be retrieved via
	 * KVM_GET_VCPU_MMAP_SIZE ioctl.
	 */
	ret = ioctl(kvmfd, KVM_GET_VCPU_MMAP_SIZE, NULL);
	vcpu_kvm_run = mmap(NULL, ret, PROT_READ | PROT_WRITE,
			    MAP_SHARED, vcpufd, 0);
	if (vcpu_kvm_run == NULL) {
		perror("Unable to mmap vcpufd");
		return -EIO;
	}

	/* Initialize vCPU */
#if defined(__arm__) || defined(__aarch64__)
	{
		struct kvm_vcpu_init vcpu_init;

		/* Initialize registers, target, etc... */
		ioctl(vmfd, KVM_ARM_PREFERRED_TARGET, &vcpu_init);
		ret = ioctl(vcpufd, KVM_ARM_VCPU_INIT, &vcpu_init);
		if (ret < 0) {
			perror("vCPU init error");
			return -EIO;
		}
	}
#elif defined(__x86_64__)
	{
		struct kvm_sregs sregs;

		/* The value of the CS register at reset is FFFF change to 0 */
		ioctl(vcpufd, KVM_GET_SREGS, &sregs);
		sregs.cs.base = 0;
		sregs.cs.selector = 0;
		ioctl(vcpufd, KVM_SET_SREGS, &sregs);

		/* IP point to start, reset RFLAGS */
		struct kvm_regs regs = {.rip = 0x0000, .rflags = 0x2 };
		ioctl(vcpufd, KVM_SET_REGS, &regs);
	}
#endif

	/* Let's go, run the vCPU */
	while (1) {
		ret = ioctl(vcpufd, KVM_RUN, NULL);
		if (ret < 0) {
			perror("KVM_RUN error");
			return -EIO;
		}

		switch (vcpu_kvm_run->exit_reason) {
		case KVM_EXIT_INTERNAL_ERROR:
			printf("Error!\n");
			return -EIO;
		case KVM_EXIT_SHUTDOWN:
		case KVM_EXIT_HLT:
			printf("Guest halted!\n");
			return 0;
		case KVM_EXIT_MMIO:
			if (vcpu_kvm_run->mmio.phys_addr == CONSOLE_FIFO_ADDR
				&& vcpu_kvm_run->mmio.is_write) {
				/* Trap & Emulate console write */
				putc(vcpu_kvm_run->mmio.data[0], stdout);
				break;
			}

			if (vcpu_kvm_run->mmio.phys_addr == MMIO_PING_CNTFRQ_ADDR
				&& vcpu_kvm_run->mmio.is_write
				&& vcpu_kvm_run->mmio.len == sizeof(uint64_t)) {
				uint64_t *val = (uint64_t *)&vcpu_kvm_run->mmio.data[0];
				cntfreq = *val;
				/* return now so guest calculate mmio latency */
				continue;
			}

			if (vcpu_kvm_run->mmio.phys_addr == MMIO_PING_LAT_ADDR) {
				uint64_t *val = (uint64_t *)&vcpu_kvm_run->mmio.data[0];
				uint64_t latns = (*val * 1000 * 1000) / (cntfreq / 1000);
				printf("mmio guest-to-guest latency %luns\n", latns);
				continue;
			}
		/* fall through */
		default:
			printf("unhandled vmexit, reason: %s\n",
			       kvm_exit_reasons[vcpu_kvm_run->exit_reason]);
			break;
		}
	}
}
