# Security policy

Report vulnerabilities privately through GitHub's security advisory feature rather than a public issue. Do not include real credentials, keys, certificates, personal health data, or device identifiers in a report.

The repository provides transport and update building blocks, not a provisioned product security boundary. Production deployment must provision unique credentials, broker ACLs, TLS trust, Secure Boot v2, flash encryption, anti-rollback/eFuse secure version, signed release images, and a validated recovery process. Credentials saved through the NVS integration APIs are only protected at rest when flash encryption is provisioned. BLE commands are deliberately rejected until a product-specific identity, pairing, authorization, and replay-protection design is implemented and validated.

Tracked source must never contain passwords, private keys, tokens, client certificates, or broker credentials. Certificate verification and hostname verification must remain enabled for production TLS.
