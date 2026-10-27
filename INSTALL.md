# Installing Inkroom

This build is for testing on your own Mac. It isn't notarized by Apple yet, so macOS needs one extra step the first time.

## 1. Copy the plugin

Download **Inkroom-mac.dmg** from the repo's Releases page (the newest build is always at `releases/latest`) and double-click it to open.

Quit Logic first. Then open Finder, press **Shift + Cmd + G**, and go to:

```
~/Library/Audio/Plug-Ins/Components
```

Drag **Inkroom.component** from the disk image into that folder. If an older Inkroom is already there, choose **Replace**.

(Optional, for other DAWs: put **Inkroom.vst3** in `~/Library/Audio/Plug-Ins/VST3`.)

## 2. Let macOS open it

Because the file came from the internet, macOS quarantines it. Open **Terminal** and paste:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Inkroom.component
```

## 3. Open it in Logic

1. Quit Logic if it's open, then reopen it. Logic scans new plugins on launch.
2. If Logic says the plugin failed validation, open **Logic Pro > Settings > Plug-in Manager**, find **Imagine Plugins > Inkroom**, and click **Reset & Rescan Selection**.
3. On an audio track or aux, click an empty insert slot and choose **Audio Units > Imagine Plugins > Inkroom**.

Inkroom follows Logic's tempo and bar position automatically. Shapes line up with bar 1, so put the loop on a drum track and press play.
