# SenselMorphTDRaw
Touchdesigner Custom Operator for Sensel Morph's Raw Data Output

License: GPL v3. 


This is a boiler-plate setup for dealing with Sensel Morph's Raw data output inside of Touchdesigner. 
It's not suppose to be pretty or user friendly. It just works if all you want is contact point(s) info. 

To test the custom op, download the SenselTests directory. It should work with the latest stable and experimental build. MAKE SURE you plug in your Sensel Morph before you open the TD project.


To compile the source code, you should grab Visual Studio Community 2019 and the Sensel Morph lib install. 

This is meant to be a jumping off point for anybody who wants to work with sensel morph.

One important note, DO NOT CALL senselSetLEDBrightness. It will slow down your code dramatically. I dropped 30 fps from just 1 contact point.
