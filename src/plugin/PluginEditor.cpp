#include "PluginEditor.h"

#include <cmath>

namespace rdui {

static juce::Font F(float h, bool bold = false) {
    return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain));
}

// ============================================================================ look and feel
LookAndFeel::LookAndFeel() {
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1019));
    setColour(juce::ComboBox::outlineColourId, col::panelEdge);
    setColour(juce::ComboBox::textColourId, col::text);
    setColour(juce::ComboBox::arrowColourId, col::dim);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff10131c));
    setColour(juce::PopupMenu::textColourId, col::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff25304a));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::PopupMenu::headerTextColourId, col::amber);
    setColour(juce::Slider::textBoxTextColourId, col::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, col::dim);
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1019));
    setColour(juce::TextEditor::textColourId, col::text);
    setColour(juce::TextEditor::outlineColourId, col::panelEdge);
    setColour(juce::AlertWindow::backgroundColourId, juce::Colour(0xff151a27));
    setColour(juce::AlertWindow::textColourId, col::text);
    setColour(juce::AlertWindow::outlineColourId, col::panelEdge);
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b2234));
    setColour(juce::TextButton::textColourOffId, col::text);
    setColour(juce::TextButton::textColourOnId, juce::Colours::black);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) {
    const float size = (float)juce::jmin(w, h);
    const float r = size * 0.5f - 3.0f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    const juce::Colour acc = s.findColour(juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc(cx, cy, r, r, 0.0f, a0, a1, true);
    g.setColour(juce::Colour(0xff222a3d));
    g.strokePath(track, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar parameters fill from the centre
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0 && std::abs(s.getMinimum() + s.getMaximum()) < 1e-6 * (s.getMaximum() - s.getMinimum()) + 1e-9;
    const float aMid = a0 + 0.5f * (a1 - a0);
    juce::Path val;
    val.addCentredArc(cx, cy, r, r, 0.0f, bipolar ? juce::jmin(aMid, ang) : a0, bipolar ? juce::jmax(aMid, ang) : ang, true);
    g.setColour(acc);
    g.strokePath(val, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float ri = r - 7.0f;
    g.setColour(juce::Colour(0xff0e121b));
    g.fillEllipse(cx - ri, cy - ri, ri * 2, ri * 2);
    g.setColour(juce::Colour(0xff2a3350));
    g.drawEllipse(cx - ri, cy - ri, ri * 2, ri * 2, 1.0f);
    const float px = cx + std::sin(ang) * (ri - 3.0f), py = cy - std::cos(ang) * (ri - 3.0f);
    const float qx = cx + std::sin(ang) * 2.0f, qy = cy - std::cos(ang) * 2.0f;
    g.setColour(acc.brighter(0.4f));
    g.drawLine(qx, qy, px, py, 2.0f);
}

void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s) {
    if (style != juce::Slider::LinearVertical) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const juce::Colour acc = s.findColour(juce::Slider::trackColourId);
    const float tx = x + w * 0.5f - 7.0f, tw = 14.0f;
    g.setColour(juce::Colour(0xff0d1019));
    g.fillRoundedRectangle(tx, (float)y, tw, (float)h, 3.0f);
    g.setColour(col::panelEdge);
    g.drawRoundedRectangle(tx, (float)y, tw, (float)h, 3.0f, 1.0f);
    const float zero = (float)s.valueToProportionOfLength(0.0);
    const float zeroY = y + h - zero * h;
    g.setColour(juce::Colour(0xff3a4560));
    g.drawLine(tx, zeroY, tx + tw, zeroY, 1.0f);
    g.setColour(acc.withAlpha(0.85f));
    const float top = juce::jmin(zeroY, pos), bot = juce::jmax(zeroY, pos);
    g.fillRect(tx + 1.0f, top, tw - 2.0f, juce::jmax(1.5f, bot - top));
    g.setColour(acc.brighter(0.6f));
    g.fillRect(tx - 1.0f, pos - 1.0f, tw + 2.0f, 2.0f);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) {
    auto r = juce::Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f);
    g.setColour(juce::Colour(down ? 0xff151b2a : 0xff0d1019));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(box.hasKeyboardFocus(true) ? col::amber : col::panelEdge);
    g.drawRoundedRectangle(r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = w - 14.0f, ay = h * 0.5f;
    arrow.addTriangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour(col::dim);
    g.fillPath(arrow);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool hl, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour c = b.getToggleState() ? col::amber : base;
    if (down) c = c.brighter(0.25f);
    else if (hl) c = c.brighter(0.12f);
    g.setColour(c);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(b.getToggleState() ? col::amber.brighter(0.4f) : col::panelEdge);
    g.drawRoundedRectangle(r, 6.0f, 1.0f);
}

// ============================================================================ controls
Knob::Knob(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& label, juce::Colour accent, int w, int h) {
    setSize(w, h);
    slider.setColour(juce::Slider::textBoxTextColourId, col::text);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    name.setText(label, juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centred);
    name.setFont(F(11.5f));
    name.setMinimumHorizontalScale(0.8f);
    addAndMakeVisible(name);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, w, 15);
    slider.setTextBoxIsEditable(true);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setPopupDisplayEnabled(false, false, nullptr);
    addAndMakeVisible(slider);
    attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, slider);
}
void Knob::resized() {
    auto r = getLocalBounds();
    name.setBounds(r.removeFromTop(15));
    slider.setBounds(r);
}

Combo::Combo(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& label, int w, int h) {
    setSize(w, h);
    name.setText(label, juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centredLeft);
    name.setFont(F(11.5f));
    addAndMakeVisible(name);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id))) box.addItemList(p->choices, 1);
    box.setJustificationType(juce::Justification::centredLeft);
    box.setColour(juce::ComboBox::textColourId, col::text);
    box.setColour(juce::ComboBox::outlineColourId, col::panelEdge);
    addAndMakeVisible(box);
    attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, id, box);
}
void Combo::resized() {
    auto r = getLocalBounds().reduced(3, 0);
    name.setBounds(r.removeFromTop(16));
    box.setBounds(r.removeFromTop(26));
}

StepSlider::StepSlider(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int index, int w, int h) : idx(index) {
    setSize(w, h);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.setColour(juce::Slider::trackColourId, col::right);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    addAndMakeVisible(slider);
    attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, slider);
}
void StepSlider::resized() { slider.setBounds(getLocalBounds().withTrimmedBottom(14)); }
void StepSlider::paint(juce::Graphics& g) {
    g.setColour(col::dim);
    g.setFont(F(11.0f));
    g.drawText(juce::String(idx + 1), getLocalBounds().removeFromBottom(14), juce::Justification::centred);
}

void Panel::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(col::panel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(col::panelEdge);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);
    g.setColour(accent_);
    g.fillRoundedRectangle(12.0f, 11.0f, 3.0f, 12.0f, 1.5f);
    g.setColour(col::text);
    g.setFont(F(12.5f, true));
    g.drawText(title_, 21, 8, getWidth() - 30, 18, juce::Justification::centredLeft);
}
void Panel::resized() {
    int y = 32;
    for (auto& row : rows_) {
        int x = 10, rowH = 0;
        for (auto* c : row) {
            c->setTopLeftPosition(x, y);
            x += c->getWidth() + 2;
            rowH = juce::jmax(rowH, c->getHeight());
        }
        y += rowH + 6;
    }
}

// ============================================================================ scope
Scope::Scope(RealityDesyncProcessor& p) : proc(p) {
    setOpaque(false);
    startTimerHz(30);
}

void Scope::timerCallback() {
    const auto& t = proc.engine.telemetry();
    s.drift = t.drift.load();
    s.cycle = t.cyclePos.load();
    s.barsToSnap = t.barsToSnap.load();
    s.bpm = t.bpm.load();
    s.synced = t.hostSynced.load();
    for (int c = 0; c < 2; ++c) {
        s.delayMs[c] = t.delayMs[c].load();
        s.pitch[c] = t.pitchSemi[c].load();
        for (int b = 0; b < rd::kMaxBands; ++b) {
            s.bandPhase[c][b] = t.bandPhase[c][b].load();
            const uint32_t n = t.bandTrig[c][b].load();
            if (n != lastTrig[c][b]) {
                lastTrig[c][b] = n;
                flash[c][b] = 1.0f;
            } else {
                flash[c][b] *= 0.82f;
            }
        }
    }
    const uint32_t sc = t.snapCount.load();
    if (sc != lastSnapCount) {
        lastSnapCount = sc;
        snapFlash = 1.0f;
    } else {
        snapFlash *= 0.9f;
    }
    // trail of where each echo actually lands relative to its grid position
    const auto& apvts = proc.apvts;
    const int idx[2] = {juce::roundToInt(apvts.getRawParameterValue("timeL")->load()), juce::roundToInt(apvts.getRawParameterValue("timeR")->load())};
    for (int c = 0; c < 2; ++c) {
        const double gridMs = rd::kDivisions[juce::jlimit(0, rd::kNumDivisions - 1, idx[c])].beats * 60000.0 / juce::jmax(20.0f, s.bpm);
        trail[c][trailPos] = (float)(s.delayMs[c] - gridMs);
    }
    trailPos = (trailPos + 1) % 96;
    phaseAnim += 0.05f;
    repaint();
}

void Scope::paint(juce::Graphics& g) {
    auto full = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0c0f17));
    g.fillRoundedRectangle(full, 8.0f);
    g.setColour(col::panelEdge);
    g.drawRoundedRectangle(full.reduced(0.5f), 8.0f, 1.0f);

    const float W = full.getWidth(), H = full.getHeight();
    const auto& apvts = proc.apvts;
    auto zoneA = juce::Rectangle<float>(14, 10, W * 0.40f - 20, H - 20);
    auto zoneB = juce::Rectangle<float>(W * 0.40f + 8, 10, W * 0.34f - 16, H - 20);
    auto zoneC = juce::Rectangle<float>(W * 0.74f + 8, 6, W * 0.26f - 20, H - 12);

    auto title = [&](juce::Rectangle<float> r, const juce::String& t) {
        g.setColour(col::dim);
        g.setFont(F(10.5f, true));
        g.drawText(t, r.getX(), r.getY() - 2, r.getWidth(), 14, juce::Justification::centredLeft);
    };

    // ------------------------------------------------ zone A: delay lanes (one bar wide)
    title(zoneA, "TIME LANES   (1 bar)");
    {
        const float x0 = zoneA.getX() + 30, x1 = zoneA.getRight();
        const float laneH = 46.0f;
        const float y0 = zoneA.getY() + 20;
        const double bpb = 4.0;
        for (int beat = 0; beat <= 4; ++beat) {
            const float x = x0 + (x1 - x0) * (float)beat / 4.0f;
            g.setColour(beat == 0 || beat == 4 ? juce::Colour(0xff3a4663) : juce::Colour(0xff1f2638));
            g.drawLine(x, y0 - 4, x, y0 + laneH * 2 + 14, beat == 0 || beat == 4 ? 1.5f : 1.0f);
        }
        for (int c = 0; c < 2; ++c) {
            const float ly = y0 + c * (laneH + 8);
            const juce::Colour colr = c == 0 ? col::left : col::right;
            g.setColour(juce::Colour(0xff10141f));
            g.fillRoundedRectangle(x0, ly, x1 - x0, laneH, 4.0f);
            const int di = juce::jlimit(0, rd::kNumDivisions - 1, juce::roundToInt(apvts.getRawParameterValue(c == 0 ? "timeL" : "timeR")->load()));
            const double gridBeats = rd::kDivisions[di].beats;
            const double gridMs = gridBeats * 60000.0 / juce::jmax(20.0f, s.bpm);
            const double nowBeats = (double)s.delayMs[c] * juce::jmax(20.0f, s.bpm) / 60000.0;
            const float gx = x0 + (float)(juce::jlimit(0.0, bpb, gridBeats) / bpb) * (x1 - x0);
            const float nx = x0 + (float)(juce::jlimit(0.0, bpb, nowBeats) / bpb) * (x1 - x0);

            // echo bar
            g.setColour(colr.withAlpha(0.20f));
            g.fillRoundedRectangle(x0, ly + 6, juce::jmax(2.0f, nx - x0), laneH - 12, 3.0f);
            // trail
            for (int k = 0; k < 96; ++k) {
                const int ix = (trailPos + k) % 96;
                const double beatsK = (gridMs + trail[c][ix]) * juce::jmax(20.0f, s.bpm) / 60000.0;
                const float tx = x0 + (float)(juce::jlimit(0.0, bpb, beatsK) / bpb) * (x1 - x0);
                g.setColour(colr.withAlpha(0.05f + 0.35f * (float)k / 96.0f));
                g.fillRect(tx - 0.5f, ly + 8 + 0.0f, 1.2f, laneH - 16);
            }
            // current position
            g.setColour(colr);
            g.fillRoundedRectangle(nx - 2.0f, ly + 3, 4.0f, laneH - 6, 2.0f);
            // grid-locked ghost
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            juce::Path tri;
            tri.addTriangle(gx - 5, ly - 1, gx + 5, ly - 1, gx, ly + 7);
            g.fillPath(tri);

            g.setColour(colr);
            g.setFont(F(11.5f, true));
            g.drawText(c == 0 ? "L" : "R", zoneA.getX(), ly, 24, laneH, juce::Justification::centred);
            const double off = s.delayMs[c] - gridMs;
            g.setColour(col::text);
            g.setFont(F(11.0f));
            g.drawText(juce::String(rd::kDivisions[di].name) + "   " + juce::String(s.delayMs[c], 1) + " ms   (" + (off >= 0 ? "+" : "") + juce::String(off, 1) + ")",
                       juce::Rectangle<float>(x0 + 6, ly + laneH - 17, x1 - x0 - 12, 14), juce::Justification::centredLeft);
        }
        g.setColour(col::dim);
        g.setFont(F(10.0f));
        g.drawText("white marker = exact musical position", x0, y0 + laneH * 2 + 14, x1 - x0, 12, juce::Justification::centredLeft);
    }

    // ------------------------------------------------ zone B: slice band clocks
    title(zoneB, "SPECTRAL SLICE CLOCKS   low -> high");
    {
        const int nb = juce::jlimit(2, rd::kMaxBands, juce::roundToInt(apvts.getRawParameterValue("sliceBands")->load()));
        const float rowH = (zoneB.getHeight() - 26) * 0.5f - 4.0f;
        for (int c = 0; c < 2; ++c) {
            const float ry = zoneB.getY() + 18 + c * (rowH + 8);
            const float cw = zoneB.getWidth() / nb;
            const juce::Colour base = c == 0 ? col::left : col::right;
            for (int b = 0; b < nb; ++b) {
                const float x = zoneB.getX() + b * cw;
                const float hue = 0.52f + 0.38f * (float)b / (float)(nb - 1);
                juce::Colour cc = c == 0 ? juce::Colour::fromHSV(juce::jlimit(0.0f, 1.0f, 0.5f + 0.12f * b / (float)nb), 0.75f, 1.0f, 1.0f) : juce::Colour::fromHSV(0.84f + 0.1f * b / (float)nb, 0.7f, 1.0f, 1.0f);
                (void)hue;
                (void)base;
                g.setColour(juce::Colour(0xff10141f));
                g.fillRoundedRectangle(x + 1.5f, ry, cw - 3.0f, rowH, 2.5f);
                const float prog = 1.0f - juce::jlimit(0.0f, 1.0f, s.bandPhase[c][b]);
                g.setColour(cc.withAlpha(0.30f + 0.5f * flash[c][b]));
                const float fh = (rowH - 4.0f) * prog;
                g.fillRoundedRectangle(x + 3.0f, ry + 2.0f + (rowH - 4.0f - fh), cw - 6.0f, juce::jmax(1.5f, fh), 2.0f);
                if (flash[c][b] > 0.05f) {
                    g.setColour(juce::Colours::white.withAlpha(flash[c][b] * 0.8f));
                    g.fillRoundedRectangle(x + 3.0f, ry + 2.0f, cw - 6.0f, 3.0f, 1.5f);
                }
            }
            g.setColour(c == 0 ? col::left : col::right);
            g.setFont(F(10.0f, true));
            g.drawText(c == 0 ? "L" : "R", zoneB.getX() - 12, ry, 10, 12, juce::Justification::centred);
        }
    }

    // ------------------------------------------------ zone C: alignment ring
    {
        const float d = juce::jmin(zoneC.getWidth(), zoneC.getHeight()) - 8.0f;
        const auto c = zoneC.getCentre();
        const float R = d * 0.5f;
        const float pi = juce::MathConstants<float>::pi;
        const float startA = -pi * 0.5f;
        g.setColour(juce::Colour(0xff1b2234));
        {
            juce::Path p;
            p.addCentredArc(c.x, c.y, R, R, 0, 0, juce::MathConstants<float>::twoPi, true);
            g.strokePath(p, juce::PathStrokeType(5.0f));
        }
        // cycle progress (time since last snap)
        {
            juce::Path p;
            p.addCentredArc(c.x, c.y, R, R, 0, startA - startA, juce::jmax(0.001f, s.cycle * juce::MathConstants<float>::twoPi), true);
            g.setColour(col::amber.withAlpha(0.9f));
            g.strokePath(p, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        // desync shape: two phase-shifted rings that separate as drift grows
        const float amp = s.drift * R * 0.30f;
        for (int k = 0; k < 2; ++k) {
            juce::Path p;
            const int N = 90;
            for (int i = 0; i <= N; ++i) {
                const float a = (float)i / N * juce::MathConstants<float>::twoPi;
                const float wob = amp * (0.55f * std::sin(a * 3.0f + phaseAnim * (k ? -1.3f : 1.0f) + k * 2.0f) + 0.45f * std::sin(a * 5.0f - phaseAnim * (k ? 0.9f : -1.4f)));
                const float rr = R * 0.62f + wob;
                const float x = c.x + std::cos(a) * rr, y = c.y + std::sin(a) * rr;
                if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
            }
            p.closeSubPath();
            g.setColour((k == 0 ? col::left : col::right).withAlpha(0.85f));
            g.strokePath(p, juce::PathStrokeType(2.0f));
        }
        if (snapFlash > 0.02f) {
            g.setColour(juce::Colours::white.withAlpha(snapFlash * 0.9f));
            g.drawEllipse(c.x - R * (0.62f + (1.0f - snapFlash) * 0.5f), c.y - R * (0.62f + (1.0f - snapFlash) * 0.5f), R * 2 * (0.62f + (1.0f - snapFlash) * 0.5f), R * 2 * (0.62f + (1.0f - snapFlash) * 0.5f), 2.5f);
        }
        g.setColour(col::text);
        g.setFont(F(15.0f, true));
        const juce::String bars = s.barsToSnap < 0 ? juce::String("free") : juce::String(juce::jmax(0.0f, s.barsToSnap), 1);
        g.drawText(bars, c.x - 40, c.y - 12, 80, 18, juce::Justification::centred);
        g.setColour(col::dim);
        g.setFont(F(9.5f));
        g.drawText(s.barsToSnap < 0 ? "no auto-snap" : "bars to snap", c.x - 40, c.y + 5, 80, 12, juce::Justification::centred);
        g.setFont(F(10.0f, true));
        g.setColour(col::dim);
        g.drawText("DESYNC " + juce::String(juce::roundToInt(s.drift * 100)) + "%", zoneC.getX(), zoneC.getBottom() - 12, zoneC.getWidth(), 12,
                   juce::Justification::centred);
        // pitch readouts
        g.setFont(F(10.5f, true));
        g.setColour(col::left);
        g.drawText("L " + juce::String(s.pitch[0], 1) + " st", zoneC.getX(), zoneC.getY() + 2, 70, 12, juce::Justification::centredLeft);
        g.setColour(col::right);
        g.drawText("R " + juce::String(s.pitch[1], 1) + " st", zoneC.getRight() - 70, zoneC.getY() + 2, 70, 12, juce::Justification::centredRight);
    }
}

}  // namespace rdui

// ============================================================================ editor
RealityDesyncEditor::RealityDesyncEditor(RealityDesyncProcessor& p) : AudioProcessorEditor(&p), proc(p), scope(p) {
    setLookAndFeel(&laf);
    setSize(1120, 836);

    using namespace rdui;
    auto& A = proc.apvts;
    auto K = [&](const char* id, const char* label, juce::Colour c, int w = 70) { return make<Knob>(A, id, label, c, w); };
    auto C = [&](const char* id, const char* label, int w) { return make<Combo>(A, id, label, w); };

    // time split
    pTime.addRow({C("timeL", "Left time", 146), C("timeR", "Right time", 146)});
    pTime.addRow({K("feedback", "Feedback", col::left, 72), K("cross", "Cross", col::left, 72), K("glide", "Glide", col::left, 72), K("timeDrift", "Wobble", col::left, 72)});

    // pitch
    pPitch.addRow({K("pitch", "Pitch", col::right, 66), K("cents", "Fine", col::right, 66), K("pitchSplit", "L/R split", col::right, 66),
                   K("pitchMix", "Amount", col::right, 66), K("grain", "Grain", col::right, 66), K("pitchDrift", "Drift", col::right, 66)});
    pPitch.addRow({C("pitchPlace", "Pitch stage", 150)});

    // desync & snap
    pDesync.addRow({K("desync", "Desync", col::amber, 70), K("driftRate", "Drift rate", col::amber, 70), K("driftBuild", "Build", col::amber, 70), K("driftCurve", "Curve", col::amber, 70)});
    pDesync.addRow({K("snapGlide", "Snap glide", col::amber, 70), C("snap", "Snap every", 150)});

    // slices
    pSlices.addRow({K("sliceMix", "Amount", col::lime, 66), K("sliceBands", "Bands", col::lime, 66), K("sliceSpread", "Spread", col::lime, 66),
                    K("sliceSkew", "L/R skew", col::lime, 66), K("sliceDecay", "Decay", col::lime, 66), K("sliceDrift", "Slip", col::lime, 66),
                    K("sliceLow", "Low", col::lime, 66), K("sliceHigh", "High", col::lime, 66)});
    pSlices.addRow({C("sliceRate", "Slice rate", 130), C("slicePattern", "Spread pattern", 130)});

    // motion
    pMotion.addRow({C("lfoShape", "LFO shape", 120), C("lfoRate", "LFO length", 100), K("lfoDepth", "Depth", col::right, 70), K("lfoPhase", "L/R phase", col::right, 70)});
    {
        std::vector<juce::Component*> row = {K("seqAmount", "Seq amount", col::right, 70), C("seqDiv", "Seq step", 100), K("seqGlide", "Seq glide", col::right, 70)};
        for (int i = 0; i < rd::kNumSeqSteps; ++i) row.push_back(make<StepSlider>(A, "seq" + juce::String(i + 1), i, 30));
        pMotion.addRow(row);
    }

    // filter
    pFilter.addRow({C("filterType", "Filter", 108), C("filterPlace", "Position", 108), C("filterModRate", "Mod length", 92), K("cutoff", "Cutoff", col::left, 66),
                    K("resonance", "Resonance", col::left, 66), K("filterMod", "Mod depth", col::left, 66)});

    // output
    pOut.addRow({K("mix", "Mix", col::amber, 72), K("outGain", "Output", col::amber, 72), K("bpm", "Int. BPM", col::amber, 72), C("clock", "Clock source", 120)});

    for (auto* pn : {&pTime, &pPitch, &pDesync, &pSlices, &pMotion, &pFilter, &pOut}) addAndMakeVisible(pn);
    addAndMakeVisible(scope);

    // header
    presetBox.setTextWhenNothingSelected("Choose a preset...");
    presetBox.onChange = [this] { presetChosen(); };
    addAndMakeVisible(presetBox);
    prevBtn.onClick = [this] { stepPreset(-1); };
    nextBtn.onClick = [this] { stepPreset(1); };
    saveBtn.onClick = [this] { promptSave(); };
    addAndMakeVisible(prevBtn);
    addAndMakeVisible(nextBtn);
    addAndMakeVisible(saveBtn);
    snapBtn.setClickingTogglesState(true);
    snapBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a2a10));
    snapBtn.setColour(juce::TextButton::buttonOnColourId, rdui::col::amber);
    snapBtn.setColour(juce::TextButton::textColourOffId, rdui::col::amber);
    snapBtn.setColour(juce::TextButton::textColourOnId, rdui::col::amber);
    snapBtn.setTooltip("Snap everything back to the grid right now");
    addAndMakeVisible(snapBtn);
    snapAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(A, "snapNow", snapBtn);

    descLabel.setFont(rdui::F(12.5f));
    descLabel.setColour(juce::Label::textColourId, rdui::col::dim);
    descLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(descLabel);
    status.setFont(rdui::F(11.5f));
    status.setColour(juce::Label::textColourId, rdui::col::dim);
    status.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(status);

    rebuildPresetMenu();
    setPresetLabel(proc.currentPresetName, proc.currentPresetDescription);
    startTimerHz(6);
}

RealityDesyncEditor::~RealityDesyncEditor() {
    stopTimer();
    snapAttach.reset();
    setLookAndFeel(nullptr);
}

void RealityDesyncEditor::timerCallback() {
    const auto& t = proc.engine.telemetry();
    const double lat = proc.engine.latencySamples() / juce::jmax(1.0, proc.getSampleRate()) * 1000.0;
    status.setText(juce::String("Clock: ") + (t.hostSynced.load() ? "host transport" : "free-running") + " @ " + juce::String(t.bpm.load(), 1) + " BPM    |    Latency: " +
                       juce::String(lat, 1) + " ms (reported to host for compensation)",
                   juce::dontSendNotification);
}

void RealityDesyncEditor::setPresetLabel(const juce::String& name, const juce::String& desc) {
    proc.currentPresetName = name;
    proc.currentPresetDescription = desc;
    descLabel.setText(desc.isEmpty() ? juce::String() : desc, juce::dontSendNotification);
    // select the matching entry without re-applying it
    ignorePresetChange = true;
    int id = 0;
    const auto& list = rdp::factoryPresets();
    for (size_t i = 0; i < menuToFactory.size(); ++i)
        if (menuToFactory[i] >= 0 && list[(size_t)menuToFactory[i]].name == name) id = (int)i + 1;
    if (id == 0)
        for (size_t i = 0; i < userFiles.size(); ++i)
            if (userFiles[i].getFileNameWithoutExtension() == juce::File::createLegalFileName(name)) id = 1000 + (int)i;
    if (id != 0) presetBox.setSelectedId(id, juce::dontSendNotification);
    else presetBox.setText(name, juce::dontSendNotification);
    currentItemId = id;
    ignorePresetChange = false;
}

void RealityDesyncEditor::rebuildPresetMenu() {
    presetBox.clear(juce::dontSendNotification);
    menuToFactory.clear();
    const auto& list = rdp::factoryPresets();
    juce::StringArray cats;
    for (auto& p : list) cats.addIfNotAlreadyThere(p.category);
    auto* menu = presetBox.getRootMenu();
    (void)menu;
    int id = 1;
    // item ids follow factory order so that prev/next can walk the list
    std::vector<std::pair<juce::String, std::vector<int>>> groups;
    for (auto& c : cats) groups.push_back({c, {}});
    menuToFactory.assign(list.size(), -1);
    for (size_t i = 0; i < list.size(); ++i) {
        menuToFactory[i] = (int)i;
        for (auto& g : groups)
            if (g.first == list[i].category) g.second.push_back((int)i);
    }
    for (auto& g : groups) {
        presetBox.addSectionHeading(g.first);
        for (int fi : g.second) presetBox.addItem(list[(size_t)fi].name, fi + 1);
    }
    (void)id;
    userFiles.clear();
    auto files = rdp::listUserPresets();
    if (files.size() > 0) {
        presetBox.addSectionHeading("User");
        for (int i = 0; i < files.size(); ++i) {
            userFiles.push_back(files[i]);
            presetBox.addItem(files[i].getFileNameWithoutExtension(), 1000 + i);
        }
    }
}

void RealityDesyncEditor::presetChosen() {
    if (ignorePresetChange) return;
    const int id = presetBox.getSelectedId();
    if (id <= 0) return;
    const auto& list = rdp::factoryPresets();
    if (id < 1000) {
        const auto& p = list[(size_t)(id - 1)];
        rdp::applyPreset(proc.apvts, p);
        setPresetLabel(p.name, p.description);
    } else {
        const int i = id - 1000;
        if (i >= 0 && i < (int)userFiles.size()) {
            juce::String n, d;
            if (rdp::loadUserPreset(proc.apvts, userFiles[(size_t)i], &n, &d)) setPresetLabel(n, d);
        }
    }
    currentItemId = id;
}

void RealityDesyncEditor::stepPreset(int dir) {
    // order: factory presets in list order, then user presets
    std::vector<int> ids;
    const int nf = (int)rdp::factoryPresets().size();
    for (int i = 0; i < nf; ++i) ids.push_back(i + 1);
    for (int i = 0; i < (int)userFiles.size(); ++i) ids.push_back(1000 + i);
    if (ids.empty()) return;
    int pos = 0;
    for (size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == currentItemId) pos = (int)i;
    if (currentItemId == 0) pos = dir > 0 ? -1 : 0;
    pos = (pos + dir + (int)ids.size()) % (int)ids.size();
    presetBox.setSelectedId(ids[(size_t)pos], juce::sendNotificationSync);
}

void RealityDesyncEditor::promptSave() {
    auto* w = new juce::AlertWindow("Save preset", "Give your preset a name and an optional description.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", proc.currentPresetName == "Init - Aligned" ? juce::String("My Preset") : proc.currentPresetName, "Name");
    w->addTextEditor("desc", "", "Description");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<RealityDesyncEditor> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w](int result) {
                           if (result != 1 || safe == nullptr) return;
                           const juce::String name = w->getTextEditorContents("name").trim();
                           const juce::String desc = w->getTextEditorContents("desc");
                           if (name.isEmpty()) return;
                           if (rdp::saveUserPreset(safe->proc.apvts, name, desc)) {
                               safe->rebuildPresetMenu();
                               safe->setPresetLabel(name, desc);
                           }
                       }),
                       true);
}

void RealityDesyncEditor::paint(juce::Graphics& g) {
    g.fillAll(rdui::col::bg);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff151b2c), 0, 0, rdui::col::bg, 0, 120, false));
    g.fillRect(0, 0, getWidth(), 120);
    g.setColour(rdui::col::text);
    g.setFont(rdui::F(22.0f, true));
    g.drawText("REALITY", 18, 8, 120, 28, juce::Justification::centredLeft);
    g.setColour(rdui::col::left);
    g.drawText("DESYNC", 106, 8, 120, 28, juce::Justification::centredLeft);
    g.setColour(rdui::col::dim);
    g.setFont(rdui::F(10.5f));
    g.drawText("time & pitch fracture", 18, 33, 180, 14, juce::Justification::centredLeft);
}

void RealityDesyncEditor::resized() {
    auto r = getLocalBounds().reduced(16, 0);
    auto head = r.removeFromTop(54).withTrimmedTop(8);
    snapBtn.setBounds(head.removeFromRight(110).reduced(0, 3));
    head.removeFromRight(10);
    saveBtn.setBounds(head.removeFromRight(70).reduced(0, 3));
    head.removeFromRight(10);
    nextBtn.setBounds(head.removeFromRight(36).reduced(0, 3));
    presetBox.setBounds(head.removeFromRight(330).reduced(0, 3));
    prevBtn.setBounds(head.removeFromRight(36).reduced(0, 3));
    head.removeFromRight(14);
    descLabel.setBounds(r.getX() + 0, 56, getWidth() - 32, 16);
    descLabel.setBounds(16, 54, getWidth() - 32, 18);

    scope.setBounds(16, 78, getWidth() - 32, 150);

    const int gap = 8;
    int y = 238;
    // row 1
    pTime.setBounds(16, y, 322, 214);
    pPitch.setBounds(16 + 322 + gap, y, 438, 214);
    pDesync.setBounds(16 + 322 + gap + 438 + gap, y, getWidth() - 32 - 322 - 438 - 2 * gap, 214);
    y += 214 + gap;
    // row 2
    pSlices.setBounds(16, y, 560, 214);
    pMotion.setBounds(16 + 560 + gap, y, getWidth() - 32 - 560 - gap, 214);
    y += 214 + gap;
    // row 3
    pFilter.setBounds(16, y, 560, 128);
    pOut.setBounds(16 + 560 + gap, y, getWidth() - 32 - 560 - gap, 128);
    status.setBounds(16, getHeight() - 22, getWidth() - 32, 18);
}
