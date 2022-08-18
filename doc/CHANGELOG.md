# Changelog

## [v1.1.2] - 2022-08-18
- Removed dependency on RapidJSON
- Unit tests are now working on Linux 
- Support for VSCode/CMake
- UnrealInitializer sources moved to simplify build process
<br>

## [v1.1.1] - 2022-05-13
- Fix an issue where the previous frame was sent to Unreal instead of the current frame. 
<br>

## [v1.1.0] - 2022-04-08
This is the first released version.
<br>

- Support for Custom Attributes. When live linking a skeleton joint from Maya to Unreal, you can choose to stream any keyframed custom attributes. 
- Support for Unreal Engine 5.0. The Unreal Engine version is selected through a new drop-down menu in the UI. 
- Support for Maya Z-up scenes. 
- Network endpoints saving 
- Support for Linux CentOS 7 
- A simplified installation experience allows you to get up-and-running in a matter of minutes. 
- The new streamlined interface lets you easily define and manage which Maya assets will be streamed to Unreal. 
- Joint hierarchy transforms can be connected, allowing for character to character animation streaming. 
- BlendShapes are supported, enabling you to make changes to characters such as facial expressions and lip syncing, and see them in-context in Unreal. 
- Camera attributes including transforms, angle of view, focal length, film gate, camera aperture, film aspect ratio, depth of field, focus distance, and fStop are supported. 
- Lighting adjustments including color, intensity, cone angle, and penumbra angle are supported. 
- Scene timecode is streamed to Unreal as metadata, allowing play head synchronization between Maya and Unreal.  
- Back version support enables the plug-in to be used with Maya 2019 through Maya 2023 on Windows. 
- Back version support enables the plug-in to be used with Maya 2020 and Maya 2023 on Linux.
