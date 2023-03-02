# Prerequisites in order to run the app:<br />
<img src="https://i.imgur.com/QT0T20e.jpeg"/><br />
<br />
**Also you will need to enable developer mode on your windows in order to work with UWP apps**<br />
<br />
# Running the prebuild version
UWP apps are not like your typical .exe apps, there is no installer per se, <br />
instead you get them from the windows store. Development builds of UWP apps can <br />
still be shared but the way you deploy them on the user's machine is a bit unusual.

The project comes with a prebuild x64 release version of the app. In order to run it,
you will have to do the following:
1. Navigate to the the "AppPackages\UWPVoxelConeTracing\UWPVoxelConeTracing_1.0.0.0_x64_Test" folder
2. Then right click on the "Add-AppDevPackage.ps1" file and click on "Run with Powershell"
3. Next just follow the onscreen prompts. Powershell will ask you if you want to install
the app's accompanying certificat, you will need to say 'yes'. UWP apps are strictly protected
and the certificat system is Microsoft's way of guaranteeing that all the apps on their store 
follow certain security guidelines. The certificat supplied here is just a default development
build one generated via visual studio when building the app.
4. After installing the app will show up on your start menu search under the name "UWPVoxelConeTracing".
It may take a few seconds to appear after the install though, make sure the search for it again in case
it doesn't show up immediately

# Screenshots:
<img src="https://i.imgur.com/gyXedDH.jpg"/><br />
<img src="https://i.imgur.com/UbtJzUx.jpg"/><br />
