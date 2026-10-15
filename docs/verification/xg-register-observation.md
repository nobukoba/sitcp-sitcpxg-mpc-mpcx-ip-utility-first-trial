# Verification record: Observed SiTCP-XG FC40..FC4F case

[Verification index](README.md) | [Current technical guide](../../FOR_DEVELOPERS.md)

These are dated observations for the stated builds and targets, not a current
specification or a new test run. Original conditions, results, recovery and
limitations are retained below. Source-relative references have been updated;
historical prose such as "above/below" refers to the original report order.
Capture/backup paths name private session storage, not files in this checkout.

## Observed SiTCP-XG FC40..FC4F case

A SiTCP-XG target at `192.168.2.187` was observed with a valid XG
identifier (`0x58544350`) and an MPCX payload that reconstructs exactly
from `FC00..FC0F + FC12..FC17`.

A historical raw observation is retained privately rather than distributing
license-derived bytes in this guide. The leading rate field was `0x83A8`.

A non-destructive read of `0xFFFFFF40..0xFFFFFF4F` returned the same
16 bytes.  Therefore these bytes are not merely an unread EEPROM remnant;
they are also visible in the corresponding runtime register window.

The first 12 bytes matched the common part of multiple verified normal
SiTCP `.mpc` files, whose layout is `MAC[6] + common[12] + varying[4]`.
This is strong empirical evidence that the observed `FC40..FC4F` contents
have a normal-MPC-formatted origin.  It is not proof of the device's write
history, and the final four bytes have not been decoded.

In particular, this observation does **not** mean that the attached MPCX
file was accidentally interpreted as an MPC file.  If the verified MPCX
payload for this target were mapped using the normal-MPC layout,
`FC40..FC4F` would instead contain `payload[6..21]`, which does not match
the observed bytes.

The public XG register map documents `FC40..FC41` as the EEPROM initial
value for the transmission-rate register, with a documented range of
1..10000.  The observed raw value `0x83A8` is 33704 and is outside that
range. Do not report it as an effective rate of 33704 Mbps. Do not
normalize, clear, or replace `FC40..FC4F` during ordinary MPCX
programming. Initialization selected by FC10 bit7 or --initialize-eeprom is the exception:
it copies runtime settings through FC4F before overlaying the MPCX payload.

A plausible history is that normal-MPC-formatted data was written to this
area before a later MPCX programming operation.  This remains a hypothesis,
not a verified fact.
