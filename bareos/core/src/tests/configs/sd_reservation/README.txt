Configuration parameters relevant for the code in reserve.cc:
- DIR::Job::PreferMountedVolumes
- SD::Device::Autoselect
- SD::Device::MediaType
- SD::Storage::DeviceReserveByMediatype


What we will need to test all cases:
- An autochanger with multiple drives
- the first drive with autoselect=no
- the second drive configured so that InitDev() fails
- the third and following drives just as is

- Another autochanger with a different media type

- A single device configured so that InitDev() fails
- A single device with a different media type
- A single working device

- A single device where IsTape() == true

All the same with DeviceReserveByMediatype enabled/disabled
The Prefer Mounted Volumes can and will be set on a per-job basis in the tests
