# RemoteThreadDetection
A Windows dll injection detection project

## Target
By detecting the creation of remote threads (that isn't done by the OS) this project
should detect in real-time any dll injection that in one way or another calls the NtCreateThreadEx API

### Known techniques that will be detected:
* CreateRemoteThread
* NtCreateUserThread
* RtlCreateUserThread
* Reflective DLL Injection
