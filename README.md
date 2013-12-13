openHEVC
========

openHEVC is a fork of Libav with only the files needed to decode HEVC content, it was created for research purposes.
Most people will not need to use this and should use the libav HEVC decoder available at https://github.com/OpenHEVC/libav instead (see https://libav.org/documentation.html for documentation).

openHEVC in combination with GPAC is used in 3 research projects:
* 4EVER
* H2B2VS
* AUSTRAL

Where is the source code of openHEVC?
--------
* openHEVC is located at https://github.com/OpenHEVC/openHEVC.
* openHEVC is under LGPL license

Where is the source code of GPAC?
--------
* gpac is located at http://gpac.wp.mines-telecom.fr.
* gpac is under LGPL license

How to compile openHEVC on linux from source code
----------
* execute these commands

```sh
git clone git://github.com/OpenHEVC/openHEVC.git
git checkout hevc
```
* install yasm
* go into OpenHEVC source folder
* execute these commands

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RELEASE ..
make
sudo make install
```

How to test openHEVC on linux from source code
----------
* Prerequisites: SDL or SDL2
* go into source folder of openHEVC
* with SDL: `cd build; ./hevc -i name_of_annexB_bitstream.(bit,bin,265)`
* with SDL2: `cd build; ./hevc_sdl2 -i name_of_annexB_bitstream.(bit,bin,265)`

How to compile gpac with openHEVC on linux
-----------
* Prerequisites (see http://gpac.wp.mines-telecom.fr/2011/04/20/compiling-gpac-on-ubuntu/)
* `svn checkout https://gpac.svn.sourceforge.net/svnroot/gpac/trunk`
* go into gpac source folder
* execute these commands

```sh
./configure 
make
sudo make install
```

How to embed HEVC into MP4 file format
-----------
* use i_main, lp_main, ld_main or ra_main bitstreams from http://ftp.kw.bbc.co.uk/hevc/hm-10.0-anchors/bitstreams/
* `MP4Box -add name_of_annexB_bitstream.(bit,bin,265) -fps 50 -new output.mp4`
  + where fps specifies the framerate (in the case of BQMall_832x480_60_qp22.bin the framerate is 60)
* `MP4Client output.mp4 # to play HEVC mp4 content`

How to embed HEVC into TS
-----------
* use i_main, lp_main, ld_main or ra_main bitstreams from http://ftp.kw.bbc.co.uk/hevc/hm-10.0-anchors/bitstreams/
* go into gpac source folder
* execute these commands:

```sh
cd bin/gcc
./mp42ts -prog=hevc.mp4 -dst-file=test.ts
MP4Client test.ts # to play HEVC transport streams
```

openHEVC contributors
-----------
* Active contributors
  + Guillaume Martres (alias smarter - google summer of code / EPFL) - student
  + Mickaël Raulet (IETR/INSA Rennes)
  + Gildas Cocherel (IETR/INSA Rennes)
  + Wassim Hamidouche (IETR/INSA Rennes)
  + Seppo Tomperi (VTT)
  + Pierre Edouard Lepere (IETR/INSA Rennes)
  + Fernando Pescador Del Oso (UPM)
  + Jesus Caño Velasco (UPM)

* Former contributors
  + Anand Meher Kotra (IETR/INSA Rennes)

- gpac contributors
-----------
* see http://gpac.wp.mines-telecom.fr/about/

