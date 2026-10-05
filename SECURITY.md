# Security policy

Report vulnerabilities privately through GitHub's security advisory feature rather than a public issue. Do not include real credentials, keys, certificates, personal health data, or device identifiers in a report.

The repository provides transport and update building blocks, not a provisioned product security boundary. Production deployment must provision unique credentials, broker authorization, TLS trust, Secure Boot v2, flash encryption, anti-rollback/eFuse secure version, signed release images, and a validated recovery process. BLE command authorization and physical-access threats require a product-specific threat model.

Tracked source must never contain passwords, private keys, tokens, client certificates, or broker credentials. Certificate verification and hostname verification must remain enabled for production TLS.
