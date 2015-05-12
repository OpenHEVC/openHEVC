openHEVC Green
========

openHEVC Green is a fork of openHEVC meant to be a power (or energy) efficient HEVC decoder.
Here we provide the user guide for the Green features only, openHEVC README is available at https://github.com/OpenHEVC/openHEVC.

openHEVC Green is a research project to improve the power consumption of an embedded HEVC decoder by reasonably degrading the image quality.
Thus, it uses ARM assembly optimisations in order to observe a speed up on the decoding process and on the energy consummed as well.
Furthermore, these optimisations are only applied on 8bit coded pixels.

What does openHEVC Green feature?
--------
* 7 (legacy), 3 et 1 taps inter-prediction luma filters
* 4 (legacy), 2 and 1 taps inter-prediction chroma filters
* Disabling of the loop filters
  + SAO on (legacy) / off
  + DBF on (legacy) / off
* Activation levels, from  0 to 12, in order to activate promptly the power-aware configuration.

How to use openHEVC Green on linux from source code
----------
* Prerequisites: SDL or SDL2
* go into source folder of openHEVC
* e.g SDL2: `cd build; ./hevc_sdl2 -i name_of_annexB_bitstream.(bit,bin,265)`
  + add `-e xxxxx` with `xxxxx` to select the configuration according to the scheme as follows:
        - first digit is the Activation Level [0-12]
        - second digit is the luma taps number, [7;3;1]
        - third digit is the chroma taps number, [4;2;1]
        - fourth digit is the desactivation of the SAO filter, [0;1]
        - fifth gidit is the desativation of the deblocking filter, [0,1]
For instance, here is how to apply a 3 taps luma interpolating filter, with a 1 tap chroma interpolating filter, with any loop filter, and this for every frame:
`./hevc_sdl2 -e 123111 -i name_of_annexB_bitstream.(bit,bin,265)`

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

Publications
-----------
* Conferences:
  + Hamidouche W., Raulet M., Déforges O, « Real time SHVC decoder: Implementation and complexity analysis », in ICIP 2014 – IEEE International Conference on Image Processing

  + Hamidouche W., Raulet M., Déforges O., « Parallel SHVC Decoder: Implementation and Analysis », in ICME 2014 – IEEE International Conference on Multimedia and Expo.
  
  + Hamidouche W., Cocherel G. , Le Feuvre J., Raulet M. and Déforges O. , « 4K Real-time video streaming with SHVC decoder and GPAC player », in ICME 2014 – IEEE International Conference on Multimedia and Expo.

  + Hamidouche W., Raulet M., Déforges O., « Multi-core software architecture for the scalable HEVC decoder »,  in ICASSP 2014 – IEEE International Conference on Acoustics, Speech, and Signal Processing.
  
  + J. Le Feuvre, J.-M. Thiesse, M. Parmentier, M. Raulet and Ch. Daguet, « Ultra high definition HEVC DASH data set », ACM MMSys, Singapore, March 2014, pp. 7-12.
  
  + M. Raulet, G. Cocherel, W. Hamidouche, J. Le Feuvre, J. Gorin, S. Kervadec and J. Viéron, « HEVC Live end to end demonstration », MMSP, Pula, Italia, October 2013. 
  
