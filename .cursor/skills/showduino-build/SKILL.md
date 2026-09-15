# showduino-build

## Purpose
Build Showduino targets using repository-verified commands only.

## Target build commands

### Active P4 Show Engine (Arduino)
`arduino-cli compile --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,ChipVariant=postv3" firmware/stage-engine-p4/ShowduinoStageEngineP4`

### Communications S3
`arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PSRAM=opi,PartitionScheme=default_8MB" firmware/s3-comms-controller/ShowduinoS3CommsController`

### C3 Pixel Node
`arduino-cli compile --fqbn "esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio" firmware/c3-pixel-node/ShowduinoC3PixelNode`

### S3 Lamp Node
`arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,FlashSize=8M,PSRAM=disabled,PartitionScheme=default_8MB" firmware/s3-lamp-node/ShowduinoS3LampNode`

### IDF P4 stage-engine project
`cd stage-engine/esp32-p4 && idf.py set-target esp32p4 && idf.py build`

### Host tests (Linux/macOS)
- `tools/protocol-tests/run_tests.sh`
- `tools/production-tests/run_tests.sh`
- `tools/storage-tests/run_tests.sh`
- `tools/plugin-bus-tests/run_tests.sh`
- `tools/e131-tests/run_tests.sh`

### WebUI embed regeneration
`python tools/embed-webui/embed_webui.py`

## Rules
- Build only impacted targets unless release validation is requested.
- Report compile success/failure per target.
- Do not claim flashing or bench verification from build-only work.
