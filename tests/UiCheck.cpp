// Drives the editor with simulated mouse and keyboard input and checks the results.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

struct InkroomEditorTest
{
    InkroomEditor& ed;
    ShapeCanvas& c;
    juce::MouseInputSource src = juce::Desktop::getInstance().getMainMouseSource();

    juce::MouseEvent ev (juce::Point<float> p, juce::ModifierKeys mods, juce::Point<float> down, int clicks = 1)
    {
        const auto t = juce::Time::getCurrentTime();
        return juce::MouseEvent (src, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, t, down, t, clicks, p != down);
    }
    juce::Point<float> at (float phase, float value)
    {
        return { c.xOfIndex (phase * ink::kN), c.yOf (value) };
    }
    void drag (juce::Point<float> a, juce::Point<float> b, juce::ModifierKeys mods, bool release = true)
    {
        c.mouseDown (ev (a, mods.withFlags (juce::ModifierKeys::leftButtonModifier), a));
        for (int i = 1; i <= 8; ++i)
            c.mouseDrag (ev (a + (b - a) * (float) i / 8.0f, mods.withFlags (juce::ModifierKeys::leftButtonModifier), a));
        if (release)
            c.mouseUp (ev (b, mods, a));
    }
    void snap (const char* file)
    {
        auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
        auto out = juce::File::getCurrentWorkingDirectory().getChildFile (file);
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    static int run();
};

int InkroomEditorTest::run()
{
    juce::ScopedJuceInitialiser_GUI init;
    InkroomProcessor proc;
    proc.loadPreset (ink::AddSpace, 0);
    std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
    auto& ed = *dynamic_cast<InkroomEditor*> (base.get());
    ed.setSize (1000, 720);
    InkroomEditorTest t { ed, ed.canvas };
    auto& c = ed.canvas;
    const auto none = juce::ModifierKeys();
    const auto cmd = juce::ModifierKeys (juce::ModifierKeys::commandModifier);
    auto& L = proc.addLanes;

    // 1. Flat lane, then add two points by clicking.
    c.setLane (ink::Level);
    L.fill (ink::Level, [] (float) { return 0.5f; });
    c.invalidate();
    t.drag (t.at (0.25f, 0.8f), t.at (0.25f, 0.8f), none);
    t.drag (t.at (0.5f, 0.8f), t.at (0.5f, 0.8f), none);
    std::cout << "Points after two clicks: " << c.nodes.size() << " (expect 4)\n";

    // 2. Cmd + drag a box around both new points selects them.
    t.drag (t.at (0.2f, 0.95f), t.at (0.55f, 0.65f), cmd);
    std::cout << "Selected by box: " << c.selectedCount() << " (expect 2)\n";

    // 3. Drag one selected point: both move together.
    const float before0 = c.nodes[1].x, before1 = c.nodes[2].x;
    t.drag (t.at (0.25f, 0.8f), t.at (0.30f, 0.6f), none, false);
    t.snap ("ui_group_drag.png");
    t.c.mouseUp (t.ev (t.at (0.30f, 0.6f), none, t.at (0.25f, 0.8f)));
    std::cout << "Group moved: " << juce::String (c.nodes[1].x - before0, 1) << " and " << juce::String (c.nodes[2].x - before1, 1)
              << " steps, value at 30% now " << juce::String (L.sample (ink::Level, 0.30), 2) << " (expect ~0.60)\n";

    // 4. Delete key removes the selection.
    t.drag (t.at (0.2f, 0.95f), t.at (0.6f, 0.4f), cmd);
    c.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
    std::cout << "Points after delete: " << c.nodes.size() << " (expect 2)\n";

    // 5. Zoom in with Cmd + scroll, then check the grid gets finer.
    const int divBefore = c.displayDivisions();
    juce::MouseWheelDetails w {}; w.deltaY = 0.6f;
    for (int i = 0; i < 4; ++i) c.mouseWheelMove (t.ev (t.at (0.5f, 0.5f), cmd, t.at (0.5f, 0.5f)), w);
    std::cout << "Zoom span " << juce::String (c.viewSpan, 3) << ", grid " << divBefore << " -> " << c.displayDivisions() << " per beat\n";
    c.zoomToBeat();
    std::cout << "Beat zoom span " << juce::String (c.viewSpan, 3) << " (expect 0.25 on a 1-bar loop)\n";

    // 6. A precise point at the "and" of beat 2 with Shift snap while zoomed.
    c.setView (0.25, 0.25);
    t.drag (t.at (0.375f, 0.9f), t.at (0.375f, 0.9f), juce::ModifierKeys (juce::ModifierKeys::shiftModifier));
    float nearest = 1e9f; for (auto& n : c.nodes) nearest = std::min (nearest, std::abs (n.x - 0.375f * ink::kN));
    std::cout << "Snapped point is " << juce::String (nearest, 2) << " steps from the 'and' of 2 (expect 0)\n";
    t.snap ("ui_zoomed.png");
    c.zoomToFit();

    // 7. Shapes and links.
    ed.shapeRepeat = 2; ed.applyShape (13);   // hexagon every beat
    std::cout << "Hexagon: start " << juce::String (L.get (ink::Level, 0), 2) << ", middle " << juce::String (L.sample (ink::Level, 0.125 / 4 * 2), 2) << "\n";
    ed.applyShape (21);                        // orbit on Pan + Size
    std::cout << "Orbit: pan at 0 = " << juce::String (L.get (ink::Pan, 0), 2) << ", size at quarter-beat = " << juce::String (L.sample (ink::Size, 0.0625), 2) << "\n";
    c.setLane (ink::Drive);
    proc.setLink (ink::AddSpace, ink::Drive, ink::Level, true);
    std::cout << "Drive linked inverted to Level: " << juce::String (L.get (ink::Drive, 100) + L.get (ink::Level, 100), 2) << " (expect 1.00)\n";
    c.setLane (ink::Level);
    t.drag (t.at (0.7f, 0.2f), t.at (0.7f, 0.2f), none);
    std::cout << "After editing Level, Drive still follows: " << juce::String (L.sample (ink::Drive, 0.7) + L.sample (ink::Level, 0.7), 2) << "\n";
    juce::MemoryBlock st; proc.getStateInformation (st);
    InkroomProcessor other; other.setStateInformation (st.getData(), (int) st.getSize());
    std::cout << "Link survives save/load: " << (other.getLink (ink::AddSpace, ink::Drive).source == ink::Level ? "yes" : "NO") << "\n";
    ed.refreshLaneButtons();

    // 8. Save and reload a user preset.
    const bool saved = proc.saveUserPreset ("UI check preset");
    auto list = proc.userPresets (ink::AddSpace);
    bool found = false; for (auto& u : list) found |= u.name == "UI check preset";
    std::cout << "User preset saved: " << (saved && found ? "yes" : "NO") << "\n";
    for (auto& u : list) if (u.name == "UI check preset") u.file.deleteFile();

    // 9. Expanded view and snapshot.
    ed.expandBtn.triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    t.snap ("ui_expanded.png");
    std::cout << "Expanded canvas width: " << c.getWidth() << "\n";
    return 0;
}

int main() { return InkroomEditorTest::run(); }
