# Standards

This folder contains the normative documents of the repository.

Most product users do not need to start here. For onboarding and normal usage,
start with:

- [Project README](../../README.md)
- [Documentation Guide](../README.md)
- [Quick Start](../QUICKSTART.md)
- [Core Concepts](../CONCEPTS.md)

Open this folder when you need one of these three things:

- the contributor engineering bar for C++17 changes
- the official generated client API contract for one authored window
- the broader interoperability contract for a replacement or generic client

## Which Standard Should You Read?

| Page | Read it when |
| --- | --- |
| [MFDStudio C++ Repository Maintenance Standard](./mfd_cpp_repository_maintenance_standard.md) | you are modifying C++, CMake, tests, or contributor-facing documentation and need the repository engineering rules |
| [MFDStudio Generated Client API Standardization](./mfd_generated_client_api_standardization.md) | you are integrating a normal C++ client specific to one authored window and want the preferred generated API surface |
| [MFDStudio External Client Interoperability Specification](./mfd_client_interoperability_specification.md) | you are building a replacement, generic, or non-generated client and need the transport and command contract |
| [MFDStudio Client Conformance Matrix](./mfd_client_conformance_matrix.md) | you want to trace interoperability requirements to automated test evidence |

## Recommended Read Order

### Contributor

1. [MFDStudio C++ Repository Maintenance Standard](./mfd_cpp_repository_maintenance_standard.md)
2. [Development Guide](../DEVELOPMENT.md)

### Window-Specific C++ Client Integrator

1. [Use The Mockup As A Client API Reference](../tutorials/11_use_the_mockup_as_a_client_api_reference.md)
2. [MFDStudio Generated Client API Standardization](./mfd_generated_client_api_standardization.md)
3. [Generated Client API Architecture](../architecture/generated_client_api.md)

### Replacement Or Generic Client Integrator

1. [MFDStudio External Client Interoperability Specification](./mfd_client_interoperability_specification.md)
2. [MFDStudio Client Conformance Matrix](./mfd_client_conformance_matrix.md)
3. [Generated Transport Map Specification](../architecture/generated_transport_map.md)

## Source Of Truth And Publication Artifacts

The versioned Markdown pages are the reviewable source of truth for repository
changes. Publication-oriented artifacts remain alongside them when needed.

- contributor standard source: [mfd_cpp_repository_maintenance_standard.md](./mfd_cpp_repository_maintenance_standard.md)
- generated API standardization source: [mfd_generated_client_api_standardization.md](./mfd_generated_client_api_standardization.md)
- interoperability specification source: [mfd_client_interoperability_specification.md](./mfd_client_interoperability_specification.md)
- conformance evidence source: [mfd_client_conformance_matrix.md](./mfd_client_conformance_matrix.md)
- Word publication draft: [MFDStudio_Generated_Client_API_Standardization.docx](./MFDStudio_Generated_Client_API_Standardization.docx)
- generic Word export script: [GenerateClientInteropSpecDocx.ps1](../GenerateClientInteropSpecDocx.ps1) with `-SourcePath docs/standards/mfd_generated_client_api_standardization.md -OutputPath docs/standards/MFDStudio_Generated_Client_API_Standardization.docx`
- Word publication draft: [MFDStudio_External_Client_Interoperability_Specification.docx](./MFDStudio_External_Client_Interoperability_Specification.docx)
- export script: [GenerateClientInteropSpecDocx.ps1](../GenerateClientInteropSpecDocx.ps1)

If you only need to get a window running, close this folder and go back to
[Quick Start](../QUICKSTART.md).
