# Credits and techniques

Haunted Hall's code is original. It uses these published techniques:

- **PolyBLEP** band-limited pulse and saw edges: V. Välimäki and A. Huovilainen,
  "Antialiasing Oscillators in Subtractive Synthesis," IEEE Signal Processing
  Magazine, 2007.
- **Topology-preserving state-variable filter**: V. Zavalishin, *The Art of VA
  Filter Design*, 2012/2018.
- **Bounded lock-free queue**: Dmitry Vyukov's bounded MPMC queue design
  (1024cores.net).
- **15-bit linear-feedback shift register noise** with long and short tap
  modes, and **4-bit, 32-step wave tables**: the general technique of early
  programmable sound generators, documented widely in public hardware
  references.
- **2-operator FM synthesis**: J. Chowning, "The Synthesis of Complex Audio
  Spectra by Means of Frequency Modulation," JAES, 1973.
- **xorshift** random numbers: G. Marsaglia, "Xorshift RNGs," JSS, 2003.
- **Rational tanh approximation** for saturation (Padé-style).

Development note: written with substantial AI assistance (Claude, Anthropic),
reviewed and directed by FlowSphereAI.
