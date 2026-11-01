// SPDX-License-Identifier: AGPL-3.0-or-later
// CARTRIDGE BROWSER: one per layer. Tabs pick the category (Drum / Bass / Lead /
// Pad), each cartridge picture is a bank, and tapping a preset loads it right
// away so you can play it. LOAD PRESET keeps it; BACK puts the old one back.
#include "HHUi.h"

namespace mt::hauntedhall::ui {

namespace {
const char* const kLayerNames[kLayers] = {"DRUM", "SYNTH A", "SYNTH B", "SYNTH C"};
const char* const kCategoryNames[int(Category::Count)] = {"KITS", "BASS", "LEAD", "PAD"};
const juce::Rectangle<int> kGhostArea{286, 92, 48, 48};
constexpr int kCarts = int(Cartridge::Count);
} // namespace

BrowserScreen::BrowserScreen(Context& c) : ScreenBase(c) {}

Param BrowserScreen::knobParam(int i) const
{
    static const ChannelParam p[4] = {ChannelParam::Decay, ChannelParam::Tone, ChannelParam::Spook, ChannelParam::Space};
    return channelParam(layer_, p[i & 3]);
}

int BrowserScreen::presetCount() const { return isDrum() ? numKitPresets() : numSynthPresets(); }
Cartridge BrowserScreen::cartridgeOf(int i) const { return isDrum() ? kitPreset(i).cartridge : synthPreset(i).cartridge; }
Category BrowserScreen::categoryOf(int i) const { return isDrum() ? Category::Drum : synthPreset(i).category; }
const char* BrowserScreen::nameOf(int i) const { return isDrum() ? kitPreset(i).name : synthPreset(i).name; }
std::set<int>& BrowserScreen::favourites() { return isDrum() ? ctx.favouriteKits : ctx.favouriteSynths; }

bool BrowserScreen::inView(int i) const { return categoryOf(i) == category_ && int(cartridgeOf(i)) == bank_; }

bool BrowserScreen::bankHasPresets(int cart) const
{
    for (int i = 0; i < presetCount(); ++i)
        if (categoryOf(i) == category_ && int(cartridgeOf(i)) == cart) return true;
    return false;
}

void BrowserScreen::listPresets()
{
    listCount_ = 0;
    for (int i = 0; i < presetCount() && listCount_ < int(list_.size()); ++i)
        if (inView(i)) list_[size_t(listCount_++)] = i;
}

void BrowserScreen::stepBank(int dir)
{
    for (int k = 1; k <= kCarts; ++k)
    {
        const int c = ((bank_ + dir * k) % kCarts + kCarts) % kCarts;
        if (bankHasPresets(c)) { bank_ = c; break; }
    }
    carouselPage_ = bank_ / 6;
    listPresets();
}

void BrowserScreen::open(Layer layer)
{
    layer_ = layer;
    original_ = selected_ = ctx.engine.presetIndex(layer);
    category_ = categoryOf(selected_);
    bank_ = int(cartridgeOf(selected_));
    carouselPage_ = bank_ / 6;
    listPresets();
    repaint();
}

void BrowserScreen::audition(int preset)
{
    selected_ = preset;
    ctx.engine.loadPreset(layer_, preset);
}

void BrowserScreen::tick()
{
    if (ticks % 4 == 0)   // ghost at 7.5 fps
    {
        ghostFrame_ = (ghostFrame_ + 1) % 8;
        repaint(kGhostArea);
    }
}

void BrowserScreen::paintScreen(juce::Graphics& g)
{
    const Assets& a = ctx.assets;
    g.drawImageAt(a.bgBrowser, 0, 0);

    // category tabs: the selected one lit, the ones this layer can't use dimmed
    for (int t = 0; t < 4; ++t)
    {
        const auto r = rect(browser::kTabs[t]);
        if (!g.clipRegionIntersects(r)) continue;
        const bool allowed = isDrum() ? t == 0 : t != 0;
        if (t == int(category_))
        {
            g.setColour(col(colour::kMint, 0.18f));
            g.fillRoundedRectangle(r.reduced(5).toFloat(), 6.0f);
            g.setColour(col(colour::kMint));
            g.drawRoundedRectangle(r.reduced(2).toFloat(), 8.0f, 3.0f);
        }
        else if (!allowed)
        {
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillRoundedRectangle(r.reduced(4).toFloat(), 6.0f);
        }
    }

    if (g.clipRegionIntersects(rect(browser::kBankName)))
    {
    g.setColour(col(colour::kMint));
    g.setFont(a.font(15.0f));
    g.drawText(juce::String(kLayerNames[int(layer_)]) + "  -  " + kCategoryNames[int(category_)] + "  -  "
                   + juce::String(cartridgeName(Cartridge(bank_))).toUpperCase(),
               rect(browser::kBankName), juce::Justification::centred);
    }

    const auto& favs = isDrum() ? ctx.favouriteKits : ctx.favouriteSynths;
    for (int k = 0; k < listCount_; ++k)
    {
        const int p = list_[size_t(k)];
        const auto r = rect(browser::kPresetSlots[k]);
        if (!g.clipRegionIntersects(r)) continue;
        const bool sel = p == selected_;
        if (sel)
        {
            g.setColour(col(colour::kMint, 0.22f));
            g.fillRoundedRectangle(r.reduced(4).toFloat(), 5.0f);
            g.setColour(col(colour::kMint));
            g.drawRoundedRectangle(r.reduced(1).toFloat(), 6.0f, 2.0f);
        }
        g.setColour(col(sel ? colour::kMint : colour::kText));
        g.setFont(a.font(14.0f));
        g.drawText(juce::String::formatted("%02d  ", p + 1) + juce::String(nameOf(p)).toUpperCase(),
                   r.withTrimmedLeft(12).withTrimmedRight(26), juce::Justification::centredLeft);
        if (favs.count(p)) g.drawImage(a.starOn, juce::Rectangle<float>(float(r.getRight() - 24), float(r.getCentreY() - 7), 14.0f, 14.0f));
    }

    const auto fav = rect(browser::kFavorite);
    if (selected_ >= 0 && favs.count(selected_))
    {
        g.setColour(juce::Colour(0xff181020));
        g.fillRect(fav.reduced(10, 4));
        g.drawImageAt(a.starOn, fav.getCentreX() - 10, fav.getCentreY() - 10);
    }

    const auto win = rect(browser::kCartWindow);
    if (g.clipRegionIntersects(win))
    {
        g.setColour(col(colour::kWindow));
        g.fillRect(win);
        g.drawImageAt(a.cartLarge[size_t(bank_)], browser::kCartArt.x, browser::kCartArt.y);
    }
    for (int i = 0; i < 4; ++i)
        if (g.clipRegionIntersects(knobBounds(browser::kKnobs[i])))
            drawKnob(g, browser::kKnobs[i], normalised(ctx.engine, knobParam(i)));

    for (int k = 0; k < 6; ++k)
    {
        const int c = carouselPage_ * 6 + k;
        if (c >= kCarts) break;
        const auto r = rect(browser::kCarouselSlots[k]);
        if (!g.clipRegionIntersects(r)) continue;
        g.drawImageAt(a.cartThumb[size_t(c)], r.getX() + (r.getWidth() - 48) / 2, r.getY() + (r.getHeight() - 48) / 2);
        if (!bankHasPresets(c))
        {
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRect(r.reduced(6));
        }
        if (c == bank_)
        {
            g.setColour(col(colour::kMint));
            g.drawRoundedRectangle(r.reduced(1).toFloat(), 6.0f, 3.0f);
        }
    }
    if (g.clipRegionIntersects(kGhostArea)) g.drawImageAt(a.ghost[size_t(ghostFrame_)], kGhostArea.getX(), kGhostArea.getY());
}

void BrowserScreen::tapped(juce::Point<int> p)
{
    const auto toastArea = rect(browser::kBankName);
    for (int t = 0; t < 4; ++t)
        if (rect(browser::kTabs[t]).contains(p))
        {
            const bool allowed = isDrum() ? t == 0 : t != 0;
            if (!allowed) { toast(isDrum() ? "DRUM LAYER: KITS ONLY" : "SYNTH LAYER: BASS, LEAD, PAD", toastArea); return; }
            category_ = Category(t);
            if (!bankHasPresets(bank_)) stepBank(1);
            listPresets();
            repaint();
            return;
        }
    if (rect(browser::kBankPrev).contains(p)) { stepBank(-1); repaint(); return; }
    if (rect(browser::kBankNext).contains(p)) { stepBank(1); repaint(); return; }
    for (int k = 0; k < listCount_; ++k)
        if (rect(browser::kPresetSlots[k]).contains(p)) { audition(list_[size_t(k)]); repaint(); return; }
    if (rect(browser::kRandom).contains(p))
    {
        int count = 0;
        for (int i = 0; i < presetCount(); ++i) count += categoryOf(i) == category_ ? 1 : 0;
        if (count == 0) return;
        int pick = juce::Random::getSystemRandom().nextInt(count);
        for (int i = 0; i < presetCount(); ++i)
            if (categoryOf(i) == category_ && pick-- == 0)
            {
                bank_ = int(cartridgeOf(i));
                carouselPage_ = bank_ / 6;
                listPresets();
                audition(i);
                break;
            }
        repaint();
        return;
    }
    if (rect(browser::kFavorite).contains(p) && selected_ >= 0)
    {
        auto& f = favourites();
        if (f.count(selected_)) f.erase(selected_);
        else f.insert(selected_);
        repaint();
        return;
    }
    if (rect(browser::kCarouselPrev).contains(p) || rect(browser::kCarouselNext).contains(p))
    {
        const int pages = (kCarts + 5) / 6;
        carouselPage_ = (carouselPage_ + (rect(browser::kCarouselNext).contains(p) ? 1 : pages - 1)) % pages;
        repaint();
        return;
    }
    for (int k = 0; k < 6; ++k)
        if (rect(browser::kCarouselSlots[k]).contains(p))
        {
            const int c = carouselPage_ * 6 + k;
            if (c >= kCarts) return;
            if (!bankHasPresets(c)) { toast("EMPTY CARTRIDGE", toastArea); return; }
            bank_ = c;
            listPresets();
            repaint();
            return;
        }
    if (rect(browser::kLoadPreset).contains(p)) { ctx.show(ScreenId::Master); return; }
    if (rect(browser::kBack).contains(p))
    {
        if (selected_ != original_) ctx.engine.loadPreset(layer_, original_);
        ctx.show(ScreenId::Master);
    }
}

} // namespace mt::hauntedhall::ui
