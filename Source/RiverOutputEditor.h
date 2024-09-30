/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2014 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __RIVEROUTPUTEDITOR_H_28EB4CC9__
#define __RIVEROUTPUTEDITOR_H_28EB4CC9__

#include <EditorHeaders.h>
#include <VisualizerEditorHeaders.h>
#include <VisualizerWindowHeaders.h>
#include "RiverOutput.h"
#include "SchemaListBox.h"

class ImageIcon;

class RiverOutputCanvas;

/**

  User interface for the RiverOutput processor.

  @see RiverOutput
*/
class RiverOutputEditor : public VisualizerEditor,
                          public Label::Listener,
                          public ComboBox::Listener,
	                      public Button::Listener
{
public:

    /** Constructor*/
    RiverOutputEditor(GenericProcessor *parentNode);

    /** Destructor */
    ~RiverOutputEditor() override = default;

    /** Create the Visualizer object*/
    Visualizer* createNewCanvas() override;

    void enable() override;
    void disable() override;

    /** UI listeners */
    void labelTextChanged(Label* label) override;
    void comboBoxChanged(ComboBox *comboBoxThatHasChanged) override;
    void buttonClicked(Button* button) override;

    /** Non-overrides */
    void refreshLabelsFromProcessor();
    void refreshSchemaFromProcessor();
    void refreshDatastreams(const Array<const DataStream *> datastreams);

    /** Return pointer to options panel */
    Component* getOptionsPanel() 
    {
        return optionsPanel.get();
    }

    void updateProcessorSchema();

private:
    ImageIcon* icon;
    bool isPlaying;

    std::unique_ptr<Label> hostnameLabel;
    std::unique_ptr<Label> hostnameLabelValue;

    std::unique_ptr<Label> portLabel;
    std::unique_ptr<Label> portLabelValue;
    std::string lastPortValue;

    std::unique_ptr<Label> passwordLabel;
    std::unique_ptr<Label> passwordLabelValue;

    std::unique_ptr<UtilityButton> connectButton;

    // OPTIONS PANEL
    RiverOutputCanvas* canvas;
    std::unique_ptr<Component> optionsPanel;
    std::unique_ptr<Label> optionsPanelTitle;
    std::unique_ptr<Label> inputTypeTitle;

    std::unique_ptr<Label> streamNameLabel;
    std::unique_ptr<Label> streamNameLabelValue;

    std::unique_ptr<Label> totalSamplesWrittenLabel;
    std::unique_ptr<Label> totalSamplesWrittenLabelValue;

    // OPTIONS PANEL: Input Type
    const int inputTypeRadioId = 1;
    std::unique_ptr<ToggleButton> inputTypeSpikeButton;
    std::unique_ptr<ToggleButton> inputTypeEventButton;

    std::unique_ptr<Label> fieldNameLabel;
    std::unique_ptr<Label> fieldNameLabelValue;

    std::unique_ptr<Label> fieldTypeLabel;
    std::unique_ptr<ComboBox> fieldTypeComboBox;

    std::unique_ptr<UtilityButton> addFieldButton;
    std::unique_ptr<UtilityButton> removeSelectedFieldButton;

    std::unique_ptr<Label> oeStreamNameLabel;
    std::unique_ptr<ComboBox> oeStreamNameComboBox;

    std::unique_ptr<SchemaListBox> schemaList;

    std::unique_ptr<Label> asyncLatencyMsLabel;
    std::unique_ptr<Label> asyncLatencyMsLabelValue;

    Label *newStaticLabel(
            const std::string& labelText,
            int boundsX,
            int boundsY,
            int boundsWidth,
            int boundsHeight) {
        return newStaticLabel(labelText, boundsX, boundsY, boundsWidth, boundsHeight, this);
    }

    static Label *newStaticLabel(
            const std::string& labelText,
            int boundsX,
            int boundsY,
            int boundsWidth,
            int boundsHeight,
            Component *parent) {
        auto *label = new Label(labelText, labelText);
        label->setBounds(boundsX, boundsY, boundsWidth, boundsHeight);
        label->setFont(Font("Small Text", 12, Font::plain));
        label->setColour(Label::textColourId, Colours::darkgrey);
        parent->addAndMakeVisible(label);
        return label;
    }

    Label *newInputLabel(
            const std::string &componentName,
            const std::string &tooltip,
            int boundsX,
            int boundsY,
            int boundsWidth,
            int boundsHeight) {
        return newInputLabel(
                componentName,
                tooltip,
                boundsX,
                boundsY,
                boundsWidth,
                boundsHeight,
                this);
    }

    static Label *newInputLabel(
            const std::string &componentName,
            const std::string &tooltip,
            int boundsX,
            int boundsY,
            int boundsWidth,
            int boundsHeight,
            Component *parent) {
        auto *label = new Label(componentName, "");
        label->setBounds(boundsX, boundsY, boundsWidth, boundsHeight);
        label->setFont(Font("Default", 15, Font::plain));
        label->setColour(Label::textColourId, Colours::white);
        label->setColour(Label::backgroundColourId, Colours::grey);
        label->setEditable(true);
        label->setTooltip(tooltip);
        parent->addAndMakeVisible(label);
        return label;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RiverOutputEditor)
};

/** 
    Visualizer for updating additional settings
*/
class RiverOutputCanvas : public Visualizer {
public:
    
    /** Constructor */
    explicit RiverOutputCanvas(GenericProcessor *n);

    /** Destructor */
    ~RiverOutputCanvas() override = default;

    /** Not used */
    void refreshState() override { }

    /** Updates labels in editor */
    void refresh() override;

    /** Starts animation callbacks*/
    void beginAnimation() override;

    /** Stops animation callbacks*/
    void endAnimation() override;

    /** Draws the background */
    void paint(Graphics& g) override;

    /** Sets viewport location */
    void resized() override;

private:
    std::unique_ptr<Viewport> viewport;
    GenericProcessor* processor;
    RiverOutputEditor* editor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RiverOutputCanvas)
};


#endif  // __RIVEROUTPUTEDITOR_H_28EB4CC9__
