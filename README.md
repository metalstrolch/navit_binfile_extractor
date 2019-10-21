# navit_binfile_extractor
NavIT binfile extractor 
Created by Metalstrolch 2019 

 usage: navit_binfile_extractor [coordinates] 

 NavIT binfile extractor extracts given area from a NavIT binfile
 It reads binfile from stdin and writes result to stdout. 

 Coordinates
 <bottom left lon> <bottom left lat> <top right lon> <top right lat>

 Example: extract Munich, Bavaria from world map
 cat world.bin | navit_binfile_extractor 11.3 47.9 11.7 48.2 > munich.bin
         
