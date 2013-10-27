zamcomp
=======

ZamComp - LV2 compressor plugin

This is a compressor plugin that adds real beef to a mix.
I have been told it resembles outboard gear when applied to drum transients.
Feel free to leave comments on my blog as all feedback is appreciated.

http://www.zamaudio.com/?p=870

Install instructions:

lv2-dev and lv2-c++-tools are required to compile this LV2 plugin

	git checkout mono
	make && sudo make install
	make clean
	git checkout stereo
	make && sudo make install
