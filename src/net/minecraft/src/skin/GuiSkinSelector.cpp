#include "GuiSkinSelector.h"
#include "GuiLoadSkinsMenu.h"
#include "SkinManager.h"
#include "GuiButton.h"
#include "Minecraft.h"
#include "FontRenderer.h"
#include "GameSettings.h"
#include "EntityPlayerSP.h"
#include "RenderEngine.h"
#include "SoundManager.h"
#include "Tessellator.h"
#include "platform/RenderAPI.h"

#include "pc/lwjgl/Keyboard.h"

#if PLATFORM_PS2 || PLATFORM_WII
#include "platform/Input.h"
#endif

#ifdef PS2_PLATFORM
#include "ps2/input/Ps2PadState.h"
#endif

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
constexpr int_t BUTTON_ID_TAB_DEFAULT = 1;
constexpr int_t BUTTON_ID_TAB_CUSTOM = 2;
constexpr int_t BUTTON_ID_PREV_SKIN = 3;
constexpr int_t BUTTON_ID_NEXT_SKIN = 4;
constexpr int_t BUTTON_ID_PLAYER2 = 10;
constexpr int_t BUTTON_ID_LOAD_SKINS = 11;
constexpr int_t BUTTON_ID_DELETE_SKIN = 12;

std::string toUpperString(const std::string &str)
{
    std::string result = str;
    for (char &c : result)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return result;
}
}

GuiSkinSelector::GuiSkinSelector(GuiScreen *parent)
    : parentScreen(parent)
    , initializedSelection(false)
    , currentPackIndex(0)
    , currentSkinIndex(0)
    , scrollOffset(0.0f)
    , dialogLeft(0)
    , dialogTop(0)
    , dialogWidth(0)
    , dialogHeight(0)
    , leftPanelWidth(0)
    , rightPanelX(0)
    , rightPanelWidth(0)
    , carouselCenterX(0)
    , carouselGroundY(0)
    , nameplateY(0)
    , nameplateHeight(0)
    , buttonTabDefault(nullptr)
    , buttonTabCustom(nullptr)
    , buttonPrevSkin(nullptr)
    , buttonNextSkin(nullptr)
    , buttonConfirm(nullptr)
    , buttonPlayer2Skin(nullptr)
    , buttonLoadSkins(nullptr)
    , buttonDeleteSkin(nullptr)
#if PLATFORM_PS2
    , ps2ActionReleaseLatch(true)
    , stickNavLatched(false)
    , dpadRepeatTimer(0)
    , stickRepeatTimer(0)
#endif
{
    SkinManager::init();
    currentPackIndex = SkinManager::getSelectedPackIndex();
    currentSkinIndex = SkinManager::getSelectedIndex();
}

void GuiSkinSelector::initGui()
{
    controlList.clear();

    if (!initializedSelection)
    {
        if (mc != nullptr && mc->gameSettings != nullptr && !mc->gameSettings->selectedSkin.empty())
        {
            SkinManager::setSelectedSkinId(mc->gameSettings->selectedSkin);
            currentPackIndex = SkinManager::getSelectedPackIndex();
            currentSkinIndex = SkinManager::getSelectedIndex();
        }
        initializedSelection = true;
    }

    if (currentPackIndex >= SkinManager::getPackCount())
        currentPackIndex = 0;

    const int totalInPack = SkinManager::getSkinCountForPack(currentPackIndex);
    if (totalInPack > 0)
    {
        if (currentSkinIndex < 0 || currentSkinIndex >= totalInPack)
            currentSkinIndex = 0;
    }

    // Dialog layout sizing
    dialogWidth = std::min<int_t>(width - 16, 384);
    dialogHeight = std::min<int_t>(height - 48, 172);
    dialogLeft = (width - dialogWidth) / 2;
    dialogTop = 12;

    leftPanelWidth = dialogWidth * 30 / 100;
    rightPanelX = dialogLeft + leftPanelWidth + 4;
    rightPanelWidth = dialogWidth - leftPanelWidth - 4;

    carouselCenterX = rightPanelX + rightPanelWidth / 2;
    nameplateHeight = 20;
    nameplateY = dialogTop + dialogHeight - nameplateHeight - 8;
    carouselGroundY = nameplateY - 6;

    // Tabs in left panel (placed below pack cover art)
    const int_t packBtnX = dialogLeft + 6;
    const int_t packBtnW = leftPanelWidth - 12;
    const int_t tabY = dialogTop + 78;

    std::string defLabel = (currentPackIndex == 0) ? "> Default Skins <" : "Default Skins";
    buttonTabDefault = new GuiButton(BUTTON_ID_TAB_DEFAULT, packBtnX, tabY, packBtnW, 18, defLabel);
    controlList.push_back(buttonTabDefault);

    if (SkinManager::getPackCount() > 1)
    {
        std::string customLabel = "Custom (" + std::to_string(SkinManager::getSkinCountForPack(1)) + ")";
        if (currentPackIndex == 1)
            customLabel = "> " + customLabel + " <";
        buttonTabCustom = new GuiButton(BUTTON_ID_TAB_CUSTOM, packBtnX, tabY + 22, packBtnW, 18, customLabel);
        controlList.push_back(buttonTabCustom);
    }
    else
    {
        buttonTabCustom = nullptr;
    }

    // Carousel buttons < and >
    buttonPrevSkin = new GuiButton(BUTTON_ID_PREV_SKIN, rightPanelX + 6, carouselGroundY - 45, 18, 20, "<");
    buttonNextSkin = new GuiButton(BUTTON_ID_NEXT_SKIN, rightPanelX + rightPanelWidth - 24, carouselGroundY - 45, 18, 20, ">");
    controlList.push_back(buttonPrevSkin);
    controlList.push_back(buttonNextSkin);

    // Bottom action buttons
    const int_t btnY = dialogTop + dialogHeight + 4;
    const int_t p2BtnWidth = 135;
    const int_t p2BtnHeight = 18;
    const int_t p2BtnX = dialogLeft + dialogWidth - p2BtnWidth;

    buttonPlayer2Skin = new GuiButton(BUTTON_ID_PLAYER2, p2BtnX, btnY, p2BtnWidth, p2BtnHeight, "Choose 2nd Player Skin");
    buttonPlayer2Skin->enabled = false;
    controlList.push_back(buttonPlayer2Skin);

    const int_t loadBtnWidth = 85;
    const int_t loadBtnX = p2BtnX - loadBtnWidth - 5;
    buttonLoadSkins = new GuiButton(BUTTON_ID_LOAD_SKINS, loadBtnX, btnY, loadBtnWidth, p2BtnHeight, "Load Skins");
    controlList.push_back(buttonLoadSkins);

    if (currentPackIndex == 1 && !SkinManager::getCustomSkins().empty())
    {
        const int_t delBtnWidth = 85;
        buttonDeleteSkin = new GuiButton(BUTTON_ID_DELETE_SKIN, dialogLeft, btnY, delBtnWidth, p2BtnHeight, "Delete Skin");
        controlList.push_back(buttonDeleteSkin);
    }
    else
    {
        buttonDeleteSkin = nullptr;
    }
}

void GuiSkinSelector::switchPack(int newPackIndex)
{
    if (newPackIndex == currentPackIndex)
        return;

    if (newPackIndex == 1 && SkinManager::getCustomSkins().empty())
        return;

    currentPackIndex = newPackIndex;
    SkinManager::setSelectedPackIndex(currentPackIndex);
    currentSkinIndex = 0;
    scrollOffset = 0.0f;

    if (mc != nullptr && mc->sndManager != nullptr)
        mc->sndManager->playSoundFX("random.click", 1.0f, 1.1f);

    initGui();
}

void GuiSkinSelector::deleteCurrentCustomSkin()
{
    if (currentPackIndex != 1)
        return;

    const auto &customs = SkinManager::getCustomSkins();
    if (customs.empty() || currentSkinIndex < 0 || currentSkinIndex >= static_cast<int>(customs.size()))
        return;

    std::string skinId = customs[currentSkinIndex].id;
    bool deleted = SkinManager::deleteCustomSkin(skinId);
    if (!deleted)
        return;

    if (mc != nullptr && mc->sndManager != nullptr)
        mc->sndManager->playSoundFX("random.break", 1.0f, 1.0f);

    // If no custom skins are left, return immediately to the main menu as requested
    if (SkinManager::getCustomSkins().empty())
    {
        cancelAndReturn();
        return;
    }

    if (currentSkinIndex >= static_cast<int>(SkinManager::getCustomSkins().size()))
        currentSkinIndex = static_cast<int>(SkinManager::getCustomSkins().size()) - 1;

    initGui();
}

void GuiSkinSelector::handleSpecializedMenuInput()
{
#if PLATFORM_PS2 || PLATFORM_WII
    const PlatformTextInputSnapshot pad = platformTextInputSnapshot(platformMenuPad());
    if (!pad.connected)
        return;

#if PLATFORM_PS2
    std::uint32_t pressed = pad.pressed;
    if (ps2ActionReleaseLatch)
    {
        pressed &= ~PLATFORM_TEXT_TYPE;
        if ((pad.held & PLATFORM_TEXT_TYPE) == 0)
            ps2ActionReleaseLatch = false;
    }

    // Cancel / Return (Circle)
    if ((pressed & PLATFORM_TEXT_CLOSE) != 0)
    {
        cancelAndReturn();
        return;
    }

    // Triangle: Open Load Skins screen
    if ((pressed & PLATFORM_TEXT_SHIFT) != 0)
    {
        if (mc != nullptr && mc->sndManager != nullptr)
            mc->sndManager->playSoundFX("random.click", 1.0f, 1.0f);
        mc->displayGuiScreen(new GuiLoadSkinsMenu(this));
        return;
    }

    // Square: Delete current custom skin (when on custom pack)
    if ((pressed & PLATFORM_TEXT_BACK) != 0)
    {
        if (currentPackIndex == 1)
        {
            deleteCurrentCustomSkin();
            return;
        }
    }

    // L1 / R1 or D-Pad Up / Down: Switch between skin packs
    const Ps2PadSnapshot &ps2Pad = ps2PadGetSnapshot(platformMenuPad());
    const Ps2PadSnapshot &primaryPad = ps2PadGetSnapshot(0);
    bool triggerPackSwitch = false;
    if ((ps2Pad.pressed & (PS2_PAD_L1 | PS2_PAD_R1)) != 0 || (primaryPad.pressed & (PS2_PAD_L1 | PS2_PAD_R1)) != 0)
        triggerPackSwitch = true;
    else if ((pressed & (PLATFORM_TEXT_UP | PLATFORM_TEXT_DOWN)) != 0)
        triggerPackSwitch = true;

    if (triggerPackSwitch && SkinManager::getPackCount() > 1)
    {
        switchPack(1 - currentPackIndex);
        return;
    }

    // D-Pad navigation with initial edge + repeat on hold
    bool movedLeft = false;
    bool movedRight = false;

    if ((pressed & PLATFORM_TEXT_LEFT) != 0)
    {
        movedLeft = true;
        dpadRepeatTimer = 0;
    }
    else if ((pad.held & PLATFORM_TEXT_LEFT) != 0)
    {
        dpadRepeatTimer++;
        if (dpadRepeatTimer > 15 && (dpadRepeatTimer % 5) == 0)
            movedLeft = true;
    }

    if ((pressed & PLATFORM_TEXT_RIGHT) != 0)
    {
        movedRight = true;
        dpadRepeatTimer = 0;
    }
    else if ((pad.held & PLATFORM_TEXT_RIGHT) != 0)
    {
        dpadRepeatTimer++;
        if (dpadRepeatTimer > 15 && (dpadRepeatTimer % 5) == 0)
            movedRight = true;
    }

    if (movedLeft) prevSkin();
    if (movedRight) nextSkin();

    // Analog stick horizontal carousel navigation
    const PlatformGamepadSnapshot stick = platformGamepadSnapshot(platformMenuPad());
    constexpr float kStickThreshold = 0.50f;
    constexpr float kStickRelease = 0.25f;

    if (stick.leftX > -kStickRelease && stick.leftX < kStickRelease)
    {
        stickNavLatched = false;
        stickRepeatTimer = 0;
    }
    else if (!stickNavLatched)
    {
        if (stick.leftX < -kStickThreshold)
        {
            prevSkin();
            stickNavLatched = true;
            stickRepeatTimer = 0;
        }
        else if (stick.leftX > kStickThreshold)
        {
            nextSkin();
            stickNavLatched = true;
            stickRepeatTimer = 0;
        }
    }
    else
    {
        stickRepeatTimer++;
        if (stickRepeatTimer > 18 && (stickRepeatTimer % 6) == 0)
        {
            if (stick.leftX < -kStickThreshold)
                prevSkin();
            else if (stick.leftX > kStickThreshold)
                nextSkin();
        }
    }

    // Confirm selection (Cross)
    if ((pressed & PLATFORM_TEXT_TYPE) != 0)
    {
        selectAndConfirm();
        return;
    }

#elif PLATFORM_WII
    // Wii Classic Controller / GameCube / Wiimote D-Pad
    if ((pad.pressed & (PLATFORM_TEXT_BACK | PLATFORM_TEXT_CLOSE)) != 0)
    {
        cancelAndReturn();
        return;
    }

    if ((pad.pressed & PLATFORM_TEXT_LEFT) != 0)
        prevSkin();
    else if ((pad.pressed & PLATFORM_TEXT_RIGHT) != 0)
        nextSkin();

    if ((pad.pressed & (PLATFORM_TEXT_UP | PLATFORM_TEXT_DOWN)) != 0 && SkinManager::getPackCount() > 1)
    {
        switchPack(1 - currentPackIndex);
        return;
    }

    if ((pad.pressed & PLATFORM_TEXT_TYPE) != 0)
    {
        selectAndConfirm();
        return;
    }
#endif
#endif
}

void GuiSkinSelector::updateScreen()
{
    GuiScreen::updateScreen();

    // Smooth return to center
    if (std::abs(scrollOffset) > 0.01f)
    {
        scrollOffset *= 0.65f;
    }
    else
    {
        scrollOffset = 0.0f;
    }
}

void GuiSkinSelector::keyTyped(char_t c, int_t key)
{
    if (key == 1) // ESC
    {
        cancelAndReturn();
        return;
    }
    if (key == lwjgl::Keyboard::KEY_RETURN || key == lwjgl::Keyboard::KEY_NUMPADENTER)
    {
        selectAndConfirm();
        return;
    }
    if (key == lwjgl::Keyboard::KEY_LEFT || key == lwjgl::Keyboard::KEY_A)
    {
        prevSkin();
        return;
    }
    if (key == lwjgl::Keyboard::KEY_RIGHT || key == lwjgl::Keyboard::KEY_D)
    {
        nextSkin();
        return;
    }
    if (key == lwjgl::Keyboard::KEY_TAB)
    {
        if (SkinManager::getPackCount() > 1)
            switchPack(1 - currentPackIndex);
        return;
    }
    if (key == lwjgl::Keyboard::KEY_DELETE)
    {
        deleteCurrentCustomSkin();
        return;
    }

    GuiScreen::keyTyped(c, key);
}

void GuiSkinSelector::nextSkin()
{
    const int total = SkinManager::getSkinCountForPack(currentPackIndex);
    if (total <= 0)
        return;

    currentSkinIndex = (currentSkinIndex + 1) % total;
    scrollOffset = -0.5f;

    if (mc != nullptr && mc->sndManager != nullptr)
        mc->sndManager->playSoundFX("random.click", 1.0f, 1.2f);
}

void GuiSkinSelector::prevSkin()
{
    const int total = SkinManager::getSkinCountForPack(currentPackIndex);
    if (total <= 0)
        return;

    currentSkinIndex = ((currentSkinIndex - 1) % total + total) % total;
    scrollOffset = 0.5f;

    if (mc != nullptr && mc->sndManager != nullptr)
        mc->sndManager->playSoundFX("random.click", 1.0f, 0.9f);
}

void GuiSkinSelector::selectAndConfirm()
{
    const SkinEntry *skin = SkinManager::getSkin(currentPackIndex, currentSkinIndex);
    if (skin != nullptr)
    {
        SkinManager::setSelectedPackIndex(currentPackIndex);
        SkinManager::setSelectedSkinId(skin->id);
        if (mc != nullptr && mc->gameSettings != nullptr)
        {
            mc->gameSettings->selectedSkin = skin->id;
            mc->gameSettings->saveOptions();
        }
        if (mc != nullptr && mc->thePlayer != nullptr)
        {
            mc->thePlayer->setEntityTexture(skin->modelPath);
            mc->thePlayer->skinUrl = "";
        }
    }

    if (mc != nullptr && mc->sndManager != nullptr)
        mc->sndManager->playSoundFX("random.action", 1.0f, 1.0f);

    mc->displayGuiScreen(parentScreen);
}

void GuiSkinSelector::cancelAndReturn()
{
    if (mc != nullptr && mc->sndManager != nullptr)
        mc->sndManager->playSoundFX("random.back", 1.0f, 1.0f);

    mc->displayGuiScreen(parentScreen);
}

void GuiSkinSelector::mouseClicked(int_t mouseX, int_t mouseY, int_t button)
{
    GuiScreen::mouseClicked(mouseX, mouseY, button);

    if (button != 0)
        return;

    // Click on nameplate confirms
    if (mouseY >= nameplateY && mouseY <= nameplateY + nameplateHeight &&
        mouseX >= rightPanelX && mouseX <= rightPanelX + rightPanelWidth)
    {
        selectAndConfirm();
        return;
    }

    // Click on center character confirms
    if (mouseX >= carouselCenterX - 25 && mouseX <= carouselCenterX + 25 &&
        mouseY >= carouselGroundY - 75 && mouseY <= carouselGroundY)
    {
        selectAndConfirm();
        return;
    }

    // Click on left skin in carousel advances to left
    if (mouseX >= carouselCenterX - 85 && mouseX < carouselCenterX - 25 &&
        mouseY >= carouselGroundY - 65 && mouseY <= carouselGroundY)
    {
        prevSkin();
        return;
    }

    // Click on right skin in carousel advances to right
    if (mouseX > carouselCenterX + 25 && mouseX <= carouselCenterX + 85 &&
        mouseY >= carouselGroundY - 65 && mouseY <= carouselGroundY)
    {
        nextSkin();
        return;
    }
}

void GuiSkinSelector::actionPerformed(GuiButton *button)
{
    if (!button->enabled)
        return;

    if (button->id == BUTTON_ID_TAB_DEFAULT)
    {
        switchPack(0);
    }
    else if (button->id == BUTTON_ID_TAB_CUSTOM)
    {
        switchPack(1);
    }
    else if (button->id == BUTTON_ID_PREV_SKIN)
    {
        prevSkin();
    }
    else if (button->id == BUTTON_ID_NEXT_SKIN)
    {
        nextSkin();
    }
    else if (button->id == BUTTON_ID_LOAD_SKINS)
    {
        if (mc != nullptr && mc->sndManager != nullptr)
            mc->sndManager->playSoundFX("random.click", 1.0f, 1.0f);
        mc->displayGuiScreen(new GuiLoadSkinsMenu(this));
    }
    else if (button->id == BUTTON_ID_DELETE_SKIN)
    {
        deleteCurrentCustomSkin();
    }
}

bool GuiSkinSelector::doesGuiPauseGame()
{
    return true;
}

// ----------------------------------------------------------------------------
// Custom Visual Drawing Helpers
// ----------------------------------------------------------------------------

void GuiSkinSelector::drawBeveledPanel(int_t left, int_t top, int_t right, int_t bottom, int_t fillColor)
{
    drawRect(left, top, right, bottom, fillColor);
    drawRect(left, top, right, top + 1, 0x40FFFFFF);
    drawRect(left, top, left + 1, bottom, 0x40FFFFFF);
    drawRect(left, bottom - 1, right, bottom, 0xFF000000);
    drawRect(right - 1, top, right, bottom, 0xFF000000);
}

void GuiSkinSelector::drawInsetPanel(int_t left, int_t top, int_t right, int_t bottom, int_t fillColor)
{
    drawRect(left, top, right, bottom, fillColor);
    drawRect(left, top, right, top + 1, 0xFF000000);
    drawRect(left, top, left + 1, bottom, 0xFF000000);
    drawRect(left, bottom - 1, right, bottom, 0x30FFFFFF);
    drawRect(right - 1, top, right, bottom, 0x30FFFFFF);
}

void GuiSkinSelector::drawFeetShadow(float centerX, float groundY, float radiusX, float radiusY, float alpha)
{
    renderEnable(RenderCapability::Blend);
    renderDisable(RenderCapability::Texture2D);
    renderBlendFunc(RenderBlendFactor::SrcAlpha, RenderBlendFactor::OneMinusSrcAlpha);

    int a = static_cast<int>(alpha * 90.0f);
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    int color = (a << 24) | 0x000000;

    int_t x1 = static_cast<int_t>(centerX - radiusX);
    int_t x2 = static_cast<int_t>(centerX + radiusX);
    int_t y1 = static_cast<int_t>(groundY - radiusY * 0.5f);
    int_t y2 = static_cast<int_t>(groundY + radiusY * 0.5f);

    drawRect(x1 + 2, y1, x2 - 2, y2, color);
    renderDisable(RenderCapability::Blend);
}

void GuiSkinSelector::drawFrontPreview(const SkinEntry *skin, float x, float y, float w, float h, float alpha)
{
    if (skin == nullptr || mc == nullptr || mc->renderEngine == nullptr)
        return;

    // If pre-rendered 16x32 front preview is present, draw single quad
    if (!skin->isCustom && !skin->frontPath.empty())
    {
        int texId = mc->renderEngine->getTexture(skin->frontPath);
        if (texId >= 0)
        {
            mc->renderEngine->bindTexture(texId);
            renderEnable(RenderCapability::Texture2D);
            renderEnable(RenderCapability::Blend);
            renderBlendFunc(RenderBlendFactor::SrcAlpha, RenderBlendFactor::OneMinusSrcAlpha);
            renderColor4f(alpha, alpha, alpha, alpha);

            Tessellator &tess = Tessellator::instance;
            tess.startDrawingQuads();
            tess.addVertexWithUV(x,     y + h, zLevel, 0.0f, 1.0f);
            tess.addVertexWithUV(x + w, y + h, zLevel, 1.0f, 1.0f);
            tess.addVertexWithUV(x + w, y,     zLevel, 1.0f, 0.0f);
            tess.addVertexWithUV(x,     y,     zLevel, 0.0f, 0.0f);
            tess.draw();
            renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            return;
        }
    }

    // Custom skin (or fallback): Assemble the 2D front character dynamically
    int texId = -1;
    if (!skin->frontPath.empty())
        texId = mc->renderEngine->getTexture(skin->frontPath);

    if (texId < 0 && !skin->skinPath.empty())
        texId = mc->renderEngine->getTexture(skin->skinPath);

    if (texId < 0 && !skin->modelPath.empty())
        texId = mc->renderEngine->getTexture(skin->modelPath);

    if (texId < 0)
        return;

    mc->renderEngine->bindTexture(texId);
    renderEnable(RenderCapability::Texture2D);
    renderEnable(RenderCapability::Blend);
    renderBlendFunc(RenderBlendFactor::SrcAlpha, RenderBlendFactor::OneMinusSrcAlpha);
    renderColor4f(alpha, alpha, alpha, alpha);

    int_t tw = 64, th = 32;
    mc->renderEngine->getTextureDimensions(texId, &tw, &th);
    if (th <= 0) th = 32;

    // If the texture loaded is already a 16x32 front preview, draw single quad
    if (tw == 16 && th == 32)
    {
        Tessellator &tess = Tessellator::instance;
        tess.startDrawingQuads();
        tess.addVertexWithUV(x,     y + h, zLevel, 0.0f, 1.0f);
        tess.addVertexWithUV(x + w, y + h, zLevel, 1.0f, 1.0f);
        tess.addVertexWithUV(x + w, y,     zLevel, 1.0f, 0.0f);
        tess.addVertexWithUV(x,     y,     zLevel, 0.0f, 0.0f);
        tess.draw();
        renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    // Assemble standing body parts from standard skin sheet
    const float uScale = 1.0f / 64.0f;
    const float vScale = 1.0f / static_cast<float>(th);
    const float unitW = w / 16.0f;
    const float unitH = h / 32.0f;

    Tessellator &tess = Tessellator::instance;
    tess.startDrawingQuads();

    auto drawQuad = [&](float qx, float qy, float qw, float qh, float su0, float sv0, float su1, float sv1) {
        tess.addVertexWithUV(qx,      qy + qh, zLevel, su0, sv1);
        tess.addVertexWithUV(qx + qw, qy + qh, zLevel, su1, sv1);
        tess.addVertexWithUV(qx + qw, qy,      zLevel, su1, sv0);
        tess.addVertexWithUV(qx,      qy,      zLevel, su0, sv0);
    };

    // 1. Head front (8,8 to 16,16)
    drawQuad(x + 4.0f * unitW, y, 8.0f * unitW, 8.0f * unitH, 8.0f * uScale, 8.0f * vScale, 16.0f * uScale, 16.0f * vScale);

    // 2. Hat overlay (40,8 to 48,16)
    drawQuad(x + 3.5f * unitW, y - 0.5f * unitH, 9.0f * unitW, 9.0f * unitH, 40.0f * uScale, 8.0f * vScale, 48.0f * uScale, 16.0f * vScale);

    // 3. Torso front (20,20 to 28,32)
    drawQuad(x + 4.0f * unitW, y + 8.0f * unitH, 8.0f * unitW, 12.0f * unitH, 20.0f * uScale, 20.0f * vScale, 28.0f * uScale, 32.0f * vScale);

    // 4. Right Arm front (44,20 to 48,32)
    drawQuad(x, y + 8.0f * unitH, 4.0f * unitW, 12.0f * unitH, 44.0f * uScale, 20.0f * vScale, 48.0f * uScale, 32.0f * vScale);

    // 5. Left Arm front
    if (th == 64)
        drawQuad(x + 12.0f * unitW, y + 8.0f * unitH, 4.0f * unitW, 12.0f * unitH, 36.0f * uScale, 52.0f * vScale, 40.0f * uScale, 64.0f * vScale);
    else
        drawQuad(x + 12.0f * unitW, y + 8.0f * unitH, 4.0f * unitW, 12.0f * unitH, 48.0f * uScale, 20.0f * vScale, 44.0f * uScale, 32.0f * vScale);

    // 6. Right Leg front (4,20 to 8,32)
    drawQuad(x + 4.0f * unitW, y + 20.0f * unitH, 4.0f * unitW, 12.0f * unitH, 4.0f * uScale, 20.0f * vScale, 8.0f * uScale, 32.0f * vScale);

    // 7. Left Leg front
    if (th == 64)
        drawQuad(x + 8.0f * unitW, y + 20.0f * unitH, 4.0f * unitW, 12.0f * unitH, 20.0f * uScale, 52.0f * vScale, 24.0f * uScale, 64.0f * vScale);
    else
        drawQuad(x + 8.0f * unitW, y + 20.0f * unitH, 4.0f * unitW, 12.0f * unitH, 8.0f * uScale, 20.0f * vScale, 4.0f * uScale, 32.0f * vScale);

    tess.draw();
    renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void GuiSkinSelector::drawScreen(int_t mouseX, int_t mouseY, float_t partialTick)
{
    (void)partialTick;

    // 1. Darkened background
    drawDefaultBackground();

    // 2. Left Panel
    const int_t leftX1 = dialogLeft;
    const int_t leftY1 = dialogTop;
    const int_t leftX2 = dialogLeft + leftPanelWidth;
    const int_t leftY2 = dialogTop + dialogHeight;
    drawInsetPanel(leftX1, leftY1, leftX2, leftY2, 0xD0101010);

    // Pack artwork container (square)
    const int_t artSize = std::min<int_t>(leftPanelWidth - 20, 60);
    const int_t artX = leftX1 + (leftPanelWidth - artSize) / 2;
    const int_t artY = leftY1 + 8;
    drawInsetPanel(artX - 1, artY - 1, artX + artSize + 1, artY + artSize + 1, 0xFF000000);

    // Draw pack cover art
    if (mc != nullptr && mc->renderEngine != nullptr)
    {
        int coverTex = mc->renderEngine->getTexture("/skins/default_pack.png");
        if (coverTex >= 0)
        {
            mc->renderEngine->bindTexture(coverTex);
            renderEnable(RenderCapability::Texture2D);
            renderColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            Tessellator &tess = Tessellator::instance;
            tess.startDrawingQuads();
            tess.addVertexWithUV(artX,           artY + artSize, zLevel, 0.0f, 1.0f);
            tess.addVertexWithUV(artX + artSize, artY + artSize, zLevel, 1.0f, 1.0f);
            tess.addVertexWithUV(artX + artSize, artY,           zLevel, 1.0f, 0.0f);
            tess.addVertexWithUV(artX,           artY,           zLevel, 0.0f, 0.0f);
            tess.draw();
        }
    }

    // 3. Right Panel (Skin Carousel and Details)
    const int_t rightX1 = rightPanelX;
    const int_t rightY1 = dialogTop;
    const int_t rightX2 = rightPanelX + rightPanelWidth;
    const int_t rightY2 = dialogTop + dialogHeight;
    drawInsetPanel(rightX1, rightY1, rightX2, rightY2, 0xD0101010);

    // Header bar inside right panel
    drawRect(rightX1 + 1, rightY1 + 1, rightX2 - 1, rightY1 + 18, 0x80282828);
    fontRenderer->drawStringWithShadow(SkinManager::getPackName(currentPackIndex), rightX1 + 10, rightY1 + 5, 0xFFFFFF);

    const int totalSkins = SkinManager::getSkinCountForPack(currentPackIndex);
    if (totalSkins > 0)
    {
        std::string countStr = std::to_string(currentSkinIndex + 1) + " / " + std::to_string(totalSkins);
        int_t countW = fontRenderer->getStringWidth(countStr);
        fontRenderer->drawStringWithShadow(countStr, rightX2 - countW - 10, rightY1 + 5, 0xAAAAAA);
    }

    // 4. Draw carousel 5 skins: -2, -1, 0, +1, +2
    if (totalSkins > 0)
    {
        struct SlotConfig { int offset; float scale; float alpha; };
        const SlotConfig slots[5] = {
            { -2, 0.45f, 0.35f },
            { -1, 0.70f, 0.70f },
            {  0, 1.00f, 1.00f },
            {  1, 0.70f, 0.70f },
            {  2, 0.45f, 0.35f },
        };

        const float spacing = 46.0f;
        const float baseW = 34.0f;
        const float baseH = 68.0f;

        for (const auto &cfg : slots)
        {
            const int skinIndex = ((currentSkinIndex + cfg.offset) % totalSkins + totalSkins) % totalSkins;
            const SkinEntry *skin = SkinManager::getSkin(currentPackIndex, skinIndex);
            if (skin == nullptr)
                continue;

            float t = static_cast<float>(cfg.offset) + scrollOffset;
            float skinW = baseW * cfg.scale;
            float skinH = baseH * cfg.scale;
            float skinX = static_cast<float>(carouselCenterX) + t * spacing - skinW / 2.0f;
            float skinY = static_cast<float>(carouselGroundY) - skinH;
            float alpha = cfg.alpha;

            drawFeetShadow(static_cast<float>(carouselCenterX) + t * spacing, static_cast<float>(carouselGroundY),
                           skinW * 0.45f, 3.5f, alpha);

            drawFrontPreview(skin, skinX, skinY, skinW, skinH, alpha);
        }
    }

    // 5. Nameplate banner
    const int_t npPadding = 8;
    const int_t npW = rightPanelWidth - npPadding * 2;
    const int_t npX = rightX1 + npPadding;
    const int_t npY = nameplateY;
    drawInsetPanel(npX, npY, npX + npW, npY + nameplateHeight, 0xFF181818);

    const SkinEntry *selectedSkin = SkinManager::getSkin(currentPackIndex, currentSkinIndex);
    if (selectedSkin != nullptr)
    {
        std::string displayName = toUpperString(selectedSkin->name);
        drawCenteredString(fontRenderer, displayName, npX + npW / 2, npY + 6, 0xFFFFAA);
    }

    // 6. Footer Controller Legend
    const int_t footerY = height - 13;
#if PLATFORM_PS2
    std::string hint = "[X] Select   [O] Back   [L1/R1] Tab   [/\\ ] Load";
    if (currentPackIndex == 1)
        hint += "   [ ] Delete";
#elif PLATFORM_WII
    std::string hint = "[A] Select   [B] Back   [L/R] Tab   [D-Pad] Navigate";
#else
    std::string hint = "[Enter] Select   [Esc] Back   [Tab] Switch Pack";
#endif
    fontRenderer->drawStringWithShadow(hint, dialogLeft, footerY, 0xC0C0C0);

    // 7. Draw standard GUI controls (buttons, tabs, arrows) AND the software cursor
    GuiScreen::drawScreen(mouseX, mouseY, partialTick);
}
