openHEVC
========


- openHEVC
----------
Fork from smarter's libav git (smarter.free.fr) with required files from libav

- openHEVC/wrapper
----------
Initial C wrapper from HM10.0 C++ reference SW (HEVC reference sw)

- openHEVC/wrapper/HM
----------
HM10.0 reference SW

- How to compile openHEVC on linux from source code
----------
* git clone git://github.com/OpenHEVC/openHEVC.git
* git checkout hm10.0
* go into OpenHEVC source folder
* mkdir build
* cd build
* cmake -DCMAKE_BUILD_TYPE=RELEASE ..
* make
* sudo make install

- How to compile gpac with openHEVC on linux
-----------
* Prerequisites (see http://gpac.wp.mines-telecom.fr/2011/04/20/compiling-gpac-on-ubuntu/)
* svn checkout -rev 4566 https://gpac.svn.sourceforge.net/svnroot/gpac/trunk
* go into gpac source folder
* ./configure
* make
* sudo make install
