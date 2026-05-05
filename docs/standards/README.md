# Interoperability Standards

This section gathers the strongest documentation artifacts of the repository:

- the repository-wide C++ maintenance and contributor standard
- the standardization note for the generated client API that normal clients are
  expected to use
- the broader interoperability specification for alternative or replacement
  clients
- the conformance matrix linking those requirements to automated evidence

Read this section when you need:

- the repository engineering bar for maintainable C++17 contributions
- the official client-facing generated API shape
- a stable client-to-window contract
- exact interoperability boundaries
- a conformance target for reviews, audits, or future formalization

## Pages

| Page | Use it when |
| --- | --- |
| [MFDStudio C++ Repository Maintenance Standard](./mfd_cpp_repository_maintenance_standard.md) | you want the default rules for C++17 design, API discipline, tests, performance, Doxygen, and contributor-facing documentation |
| [MFDStudio Generated Client API Standardization](./mfd_generated_client_api_standardization.md) | you want the preferred generated client surface, its feature coverage, and the authoring rules that drive generation |
| [MFDStudio External Client Interoperability Specification](./mfd_client_interoperability_specification.md) | you want the replacement-client contract, transport rules, command semantics, and conformance checklist |
| [MFDStudio Client Conformance Matrix](./mfd_client_conformance_matrix.md) | you want to know which interoperability requirements are already covered by automated tests and which gaps remain |

## Publication Artifacts

- contributor standard source: [mfd_cpp_repository_maintenance_standard.md](./mfd_cpp_repository_maintenance_standard.md)
- generated API standardization source: [mfd_generated_client_api_standardization.md](./mfd_generated_client_api_standardization.md)
- source specification: [mfd_client_interoperability_specification.md](./mfd_client_interoperability_specification.md)
- Word publication draft: [MFDStudio_External_Client_Interoperability_Specification.docx](./MFDStudio_External_Client_Interoperability_Specification.docx)
- export script: [GenerateClientInteropSpecDocx.ps1](../GenerateClientInteropSpecDocx.ps1)

If you are new to the project, start first with:

- [Project README](../../README.md)
- [Documentation Guide](../README.md)
- [Quick Start](../QUICKSTART.md)
- [Core Concepts](../CONCEPTS.md)
- [MFDStudio C++ Repository Maintenance Standard](./mfd_cpp_repository_maintenance_standard.md)

If you are integrating one C++ client specific to one authored window, start
with the generated-client standardization page above before the broader
replacement-client specification.
