openHEVC Green
========

openHEVC Green is a fork of openHEVC meant to be a power (or energy) efficient HEVC decoder.
Here we provide the user guide for the Green features only, openHEVC README is available at https://github.com/OpenHEVC/openHEVC.

openHEVC Green is a research project to improve the power consumption of an embedded HEVC decoder by reasonably degrading the image quality.
Thus, it uses ARM NEON assembly optimisations in order to observe a speed up on the decoding process and on the energy consummed as well.
Furthermore, these optimisations are only applied on 8bit coded pixels.

In order to reduce the power consumption of the decoder the main goal was to simplify the inter-prediction filters in one hand, and in the other hand to desactivate the in-loop filters.

What does openHEVC Green feature?
--------
* 7 (legacy), 3 et 1 taps inter-prediction luma filters
* 4 (legacy), 2 and 1 taps inter-prediction chroma filters
* Disabling of the in-loop filters
  + SAO on (legacy) / off
  + DBF on (legacy) / off
* Activation levels, from  0 to 12, in order to activate promptly the power-aware configuration.

How to use openHEVC Green on linux from source code
----------
* Prerequisites: SDL or SDL2
* go into source folder of openHEVC
* e.g SDL2: `cd build; ./hevc_sdl2 -i name_of_annexB_bitstream.(bit,bin,265)`
  + add `-e alcsd` with `alcsd` to select the configuration according to the scheme as follows:
        - a is the Activation Level [0-12]
        - l is the luma taps number, [7;3;1]
        - c is the chroma taps number, [4;2;1]
        - s is the activation of the SAO filter, [0;1]
        - d is the activation of the deblocking filter, [0,1]
  + -E enables the verbose mode with the same arguments as above

For instance, here is how to apply 3 taps luma interpolating filters, with 1 tap chroma interpolating filters, without any loop filter, and this for every frame:
`./hevc_sdl2 -e 123100 -i name_of_annexB_bitstream.(bit,bin,265)`


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
  + Morgan Lacour (IETR/INSA Rennes trainee)

* Former contributors
  + Anand Meher Kotra (IETR/INSA Rennes)


Publications
-----------
* Conferences:
  + Erwan Nogues, Simon Holmbacka, Maxime Pelcat, Daniel Menard, Johan Lilius. Power-Aware HEVC Decoding with Tunable Image Quality. IEEE International Workshop on Signal Processing Systems, Oct 2014, Belfast, United Kingdom.

  + Erwan Nogues, Morgan Lacour, Erwan Raffin, Maxime Pelcat, Daniel Menard. Low Power Software HEVC Decoder Demo for Mobile Devices. ICME 2015.

