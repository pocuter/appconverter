# APPConverter

A converter to convert a ESP32 flash file into a pocuter app

appconverter.exe

    -image   [image filename]
   
    -version [X.Y.Z]
   
    -meta    [metadata filename]
   
    -sign    [private key filename]
   
    -id      [APP-ID]
   
    -help


<h3>Meta file example (it's an ini file):</h3>

[APPDATA]<br/>
Name=DOOM II<br/>
Author=id Software<br/>


<h3>Getting the image file in Arduino:</h3>
Go to "Sketch" / "Export compiled binary" and after the Sketch is compiled, you will find the bin file in your Sketch directory.

<h3>APP-ID</h3>
The App-Id should be an unique ID for your app. Please select a number greater than 100000 to avoid overlapping with official apps. If your app makes it to the app store the number will change.
