# Simple Anti-Splicer 

## About

This solution is created as a demonstration of one of the anti-rootkit techniques, and in particular it illustrates the detection and remediation against malware hooks set up by the splicing method.

Splicing is a method of API function hooking by changing the code of the target function. Usually it is used to hide some files and/or processes in the system. Typically, the malware changes the first 5 bytes of the target function inserting a jump to the custom function. [This article](https://www.apriorit.com/dev-blog/235-splicing-to-hide-files-and-processes) explains and illustrates this approach.

## Implementation

The implemented approach is based on the verification of the entire ntoskernel image, similar to the way it is performed in the windbg !chkimg extention. Verification algorithm repeats some actions of the PE loader. 

After verification, all detected hooks are removed. 

You can find step-by-step code explanations and approach details in the [related article](https://www.apriorit.com/dev-blog/236-simple-anti-splicer).

## License

Licensed under the MIT license. © Apriorit.
