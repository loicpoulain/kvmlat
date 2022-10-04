# kvmlat

kvmlat measures the latency for the guest-to-guest round-trip. The latency is the
overhead introduced by kvm and the VMM. In ARM context, it's the time the cpu will
take for the following trip (non VHE):
EL1 -> KVM EL2 -> KVM EL1 -> VMM EL0 -> KVM EL1 -> KVM EL2 -> Guest EL1.

Execute ./kvmlat

```
./kvmlat
Hello World, from guest!
mmio guest-to-guest latency 7625ns
mmio guest-to-guest latency 6500ns
mmio guest-to-guest latency 5875ns
mmio guest-to-guest latency 5750ns
mmio guest-to-guest latency 5875ns
mmio guest-to-guest latency 6125ns
mmio guest-to-guest latency 6125ns
mmio guest-to-guest latency 6000ns
mmio guest-to-guest latency 5750ns
mmio guest-to-guest latency 5750ns
mmio guest-to-guest latency 5625ns
mmio guest-to-guest latency 5750ns
mmio guest-to-guest latency 6000ns
mmio guest-to-guest latency 5625ns
```
