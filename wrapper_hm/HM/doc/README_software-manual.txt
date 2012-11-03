Software manual instructions
============================
The software manual is written in plain text using LaTeX markup.

Prerequisites
-------------
The following tools are required to render the document:
  - LaTeX
  - JCT-VC document template

The document uses the JCT-VC report class/template, available from:
  http://hevc.kw.bbc.co.uk/git/w/jctvc-latex.git

To install this, either --
 a) export the environment variable TEXINPUTS=path/to/jctvc-latex/::
 b) copy jctvcdoc.cls to this directory.

NB, if performing (b), please do not commit the jctvcdoc.cls file.

Building
--------
A makefile is provided that will render a pdf from the LaTeX source.
If LaTeX is installed, typing "make" ought to be sufficient.

Please do not commit updated PDFs to the SVN repository, this will be
performed by the Software AHG prior to making an HM release.

If there are any issues with the building the document or formatting
the LaTeX source, please contact David Flynn <davidf@rd.bbc.co.uk>.
