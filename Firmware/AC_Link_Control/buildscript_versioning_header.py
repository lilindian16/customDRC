import datetime
FILENAME_BUILDNO = 'versioning'
FILENAME_VERSION_H = 'include/version.h'
version = 'v0.1.'


build_no = 0
try:
    with open(FILENAME_BUILDNO) as f:
        build_no = int(f.readline()) + 1
except:
    print('Starting build number from 1..')
    build_no = 1
with open(FILENAME_BUILDNO, 'w+') as f:
    f.write(str(build_no))
    print('Build number: {}'.format(build_no))

time = datetime.datetime.now()

hf = """
#ifndef BUILD_NUMBER
  #define BUILD_NUMBER "{}"
#endif
#ifndef FW_VERSION
  #define FW_VERSION "{} - {}"
#endif
#ifndef FW_VERSION_SHORT
  #define FW_VERSION_SHORT "{}"
#endif
""".format(build_no, version+str(build_no), time, version+str(build_no))
with open(FILENAME_VERSION_H, 'w+') as f:
    f.write(hf)

build_string_format = f"{version+str(build_no)}"
with open(FILENAME_BUILDNO, 'a') as f:
    f.write("\n" + str(build_string_format))
    print('Version number: {}'.format(build_string_format))
