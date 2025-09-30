### Requirements

 - A display capable of VRR (GSync / FreeSync) and a refresh rate higher than 60 Hz
 - A system capable of permanently keeping the game above 60 FPS
 - A bit of experience in terms of installing mods and changing your GPU settings (AKA: PC Gaming :))

### Preparations

 1. Check the game settings:

	 - Display mode: **Fullscreen**
	 - Vsync: **Off**
	 - Refresh Rate: **At least 60 (59 doesn't work!)**
	 
 2. Check your driver settings for DR 2.0:

	 - Variable Refresh Rate / GSync must be **enabled**
	 - VSync should be on "**Fast Sync**" (Nvidia) or "**Enhanced Sync**" (AMD)
	 - Frame Limiters must be **disabled**

3. Launch a Rallye stage with a FPS measurement tool enabled (easiest:
    Steam) and make sure that
    	 - the game's running at or above 60 Hz (as mentioned, 59 FPS is not enough) 
    	 - the game doesn't suffer from vertical tearing

### Installation

 1. Download the latest Release from [here](https://github.com/uilchtchuirn/DR2-SmoothCockpitCam/releases/)
 2. Download Special K from https://www.special-k.info/ and install it
   (create an icon)
 3. Launch Special K and click on Config folder: Centralized, then close
   Special K but leave the Explorer window open
 4. Copy the contents of the folder "Special K" from DR2-SmoothCockpitCam
   onto their respective locations in the Special K explorer
   window, overwrite if asked
 5. Copy DR2Tools.dll and DR2Tools.cfg from the archive's DR2Tools folder
   into the DiRT Rally 2.0 game directory (where dirtrally2.exe is
   located)

### Usage

 - Launch Special K and click on "Service" in order to enable the
   Injection service. Consider removing the checkbox "Stop
   automatically"
 - Launch DiRT Rally 2.0
 - Make sure that the message "Special K Plugin Loaded: DR2Tools.dll"
   appears at the bottom left
 - Launch a Rallye, and, once you're in the cockpit cam, enable the
   camera (Keybord: Insert / Gamepad: press down the left thumb stick /
   Wheel: Press button 13

### Customization

 - If you want to change the level of smoothing or change the button binding for gamepad / wheel, edit DR2Tools.cfg  from the DR2.0 main directory accordingly (instructions inside)