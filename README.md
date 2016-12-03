Scheduled CRAX Fuzzer
=====================

###Directory Structure:
1. s2e - Modified s2e code for CRAX Fuzzer.
2. s2e\_out - Use to store execution result of s2e. It also contains some scripts used to run s2e conveniently.
3. crax-fuzzer - Files needed by CRAX Fuzzer. It includes:
    * host - Files needed in host.
    * guest, ch\_guest - Files needed in guest.
    * verify, ch\_verify - Scripts and results used to verify exection of (Scheduled) CRAX Fuzzer. Needed to be placed in verify edge.
    * pool - Source of benchmrak of CRAX Fuzzer.
    * ch\_pool - New test sources.
4. configs - Configure files in lua format for s2e mode execution.
5. RL\_codes - Codes for Reiforcement learning. (by RLPy)

* Scheduled CRAX Fuzzer uses *ch\_guest* and *ch\_verify* folders.
* Guest edge and verify edge are builded by the image of the same os.
