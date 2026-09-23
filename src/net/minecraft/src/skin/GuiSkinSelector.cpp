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
    , tabDefaultTop(0)
    , tabDefaultBottom(0)
    , tabCustomTop(0)
    , tabCustomBottom(0)
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

    if (mc != nullptr && mc->gameSettings != nullptr && !mc->gameSettings->selectedSkin.empty())
    {
        SkinManager::setSelectedSkinId(mc->gameSettings->selectedSkin);
        currentPackIndex = SkinManager::getSelectedPackIndex();
        currentSkinIndex = SkinManager::getSelectedIndex();
    }

    // Dialog layout sizing
    dialogWidth = std::min<int_t>(width - 24, 380);
    dialogHeight = std::min<int_t>(height - 40, 206);
    dialogLeft = (width - dialogWidth) / 2;
    dialogTop = (height - 20 - dialogHeight) / 2;

    leftPanelWidth = dialogWidth * 30 / 100;
    rightPanelX = dialogLeft + leftPanelWidth + 4;
    rightPanelWidth = dialogWidth - leftPanelWidth - 4;

    carouselCenterX = rightPanelX + rightPanelWidth / 2;
    nameplateHeight = 28;
    nameplateY = dialogTop + dialogHeight - nameplateHeight - 10;
    carouselGroundY = nameplateY - 8;

    // Tabs vertical placement in left panel
    tabDefaultTop = dialogTop + 96;
    tabDefaultBottom = tabDefaultTop + 24;
    tabCustomTop = tabDefaultBottom + 6;
    tabCustomBottom = tabCustomTop + 24;

    // Bottom action buttons
    const int_t p2BtnWidth = 135;
    const int_t p2BtnHeight = 18;
    const int_t p2BtnX = width - p2BtnWidth - 8;
    const int_t p2BtnY = height - p2BtnHeight - 4;

    buttonPlayer2Skin = new GuiButton(BUTTON_ID_PLAYER2, p2BtnX, p2BtnY, p2BtnWidth, p2BtnHeight, "Choose 2nd Player Skin");
    buttonPlayer2Skin->enabled = false; // Disabled (grayed out) temporarily
    controlList.push_back(buttonPlayer2Skin);

    // Functional "Load Skins" button to the left of Player 2 button as requested
    const int_t loadBtnWidth = 90;
    const int_t loadBtnX = p2BtnX - loadBtnWidth - 6;
    buttonLoadSkins = new GuiButton(BUTTON_ID_LOAD_SKINS, loadBtnX, p2BtnY, loadBtnWidth, p2BtnHeight, "Load Skins");
    controlList.push_back(buttonLoadSkins);

    // "Delete Skin" button (visible when on custom pack)
    if (currentPackIndex == 1 && !SkinManager::getCustomSkins().empty())
    {
        const int_t delBtnWidth = 85;
        const int_t delBtnX = dialogLeft + 4;
        buttonDeleteSkin = new GuiButton(BUTTON_ID_DELETE_SKIN, delBtnX, p2BtnY, delBtnWidth, p2BtnHeight, "Delete Skin");
        controlList.push_back(buttonDeleteSkin);
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
    bool triggerPackSwitch = false;
    if ((ps2Pad.pressed & (PS2_PAD_L1 | PS2_PAD_R1)) != 0)
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
        if (dpadRepeatTimer >= 16 && (dpadRepeatTimer % 5 == 0))
            movedLeft = true;
    }
    else if ((pressed & PLATFORM_TEXT_RIGHT) != 0)
    {
        movedRight = true;
        dpadRepeatTimer = 0;
    }
    else if ((pad.held & PLATFORM_TEXT_RIGHT) != 0)
    {
        dpadRepeatTimer++;
        if (dpadRepeatTimer >= 16 && (dpadRepeatTimer % 5 == 0))
            movedRight = true;
    }
    else
    {
        dpadRepeatTimer = 0;
    }

    // Left analog stick navigation (deadzone + latch + repeat)
    if (!movedLeft && !movedRight)
    {
        const PlatformGamepadSnapshot stick = platformGamepadSnapshot(platformMenuPad());
        if (stick.connected)
        {
            if (stick.leftX < -0.55f)
            {
                if (!stickNavLatched)
                {
                    movedLeft = true;
                    stickNavLatched = true;
                    stickRepeatTimer = 0;
                }
                else
                {
                    stickRepeatTimer++;
                    if (stickRepeatTimer >= 16 && (stickRepeatTimer % 5 == 0))
                        movedLeft = true;
                }
            }
            else if (stick.leftX > 0.55f)
            {
                if (!stickNavLatched)
                {
                    movedRight = true;
                    stickNavLatched = true;
                    stickRepeatTimer = 0;
                }
                else
                {
                    stickRepeatTimer++;
                    if (stickRepeatTimer >= 16 && (stickRepeatTimer % 5 == 0))
                        movedRight = true;
                }
            }
            else
            {
                stickNavLatched = false;
                stickRepeatTimer = 0;
            }
        }
    }

    if (movedLeft)
        prevSkin();
    else if (movedRight)
        nextSkin();

    // Cross / Confirm
    if ((pressed & PLATFORM_TEXT_TYPE) != 0)
    {
        selectAndConfirm();
    }
#elif PLATFORM_WII
    if ((pad.pressed & PLATFORM_TEXT_LEFT) != 0)
        prevSkin();
    else if ((pad.pressed & PLATFORM_TEXT_RIGHT) != 0)
        nextSkin();
    if ((pad.pressed & (PLATFORM_TEXT_UP | PLATFORM_TEXT_DOWN)) != 0 && SkinManager::getPackCount() > 1)
        switchPack(1 - currentPackIndex);
    if ((pad.pressed & PLATFORM_TEXT_TYPE) != 0)
        selectAndConfirm();
    if ((pad.pressed & PLATFORM_TEXT_CLOSE) != 0)
        cancelAndReturn();
#endif
#endif
}

void GuiSkinSelector::updateScreen()
{
    GuiScreen::updateScreen();

    // Smoothly dampen carousel scrolling transition
    if (std::fabs(scrollOffset) > 0.001f)
    {
        scrollOffset *= 0.65f;
        if (std::fabs(scrollOffset) < 0.002f)
            scrollOffset = 0.0f;
    }
}

void GuiSkinSelector::keyTyped(char_t, int_t key)
{
    if (key == lwjgl::Keyboard::KEY_ESCAPE)
    {
        cancelAndReturn();
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
    if (key == lwjgl::Keyboard::KEY_UP || key == lwjgl::Keyboard::KEY_W ||
        key == lwjgl::Keyboard::KEY_DOWN || key == lwjgl::Keyboard::KEY_S ||
        key == lwjgl::Keyboard::KEY_TAB)
    {
        if (SkinManager::getPackCount() > 1)
            switchPack(1 - currentPackIndex);
        return;
    }
    if (key == lwjgl::Keyboard::KEY_DELETE)
    {
        if (currentPackIndex == 1)
            deleteCurrentCustomSkin();
        return;
    }
    if (key == lwjgl::Keyboard::KEY_RETURN || key == lwjgl::Keyboard::KEY_SPACE)
    {
        selectAndConfirm();
        return;
    }
}

void GuiSkinSelector::nextSkin()
{
    const int total = SkinManager::getSkinCountForPack(currentPackIndex);
    if (total <= 0)
        return;
    currentSkinIndex = (currentSkinIndex + 1) % total;
    scrollOffset -= 1.0f;
    if (scrollOffset < -1.5f)
        scrollOffset = -1.5f;

    if (mc != nullptr && mc->sndManager != nullptr)
        mc->sndManager->playSoundFX("random.click", 1.0f, 1.2f);
}

void GuiSkinSelector::prevSkin()
{
    const int total = SkinManager::getSkinCountForPack(currentPackIndex);
    if (total <= 0)
        return;
    currentSkinIndex = ((currentSkinIndex - 1) % total + total) % total;
    scrollOffset += 1.0f;
    if (scrollOffset > 1.5f)
        scrollOffset = 1.5f;

    if (mc != nullptr && mc->sndManager != nullptr)
        mc->sndManager->playSoundFX("random.click", 1.0f, 1.2f);
}

void GuiSkinSelector::selectAndConfirm()
{
    const SkinEntry *skin = SkinManager::getSkin(currentPackIndex, currentSkinIndex);
    if (skin != nullptr)
    {
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

    // Check clicks on pack tabs in left panel
    if (mouseX >= dialogLeft + 6 && mouseX <= dialogLeft + leftPanelWidth - 6)
    {
        if (mouseY >= tabDefaultTop && mouseY <= tabDefaultBottom)
        {
            switchPack(0);
            return;
        }
        if (SkinManager::getPackCount() > 1 && mouseY >= tabCustomTop && mouseY <= tabCustomBottom)
        {
            switchPack(1);
            return;
        }
    }

    // Check click inside carousel area
    if (mouseY >= dialogTop + 24 && mouseY < nameplateY)
    {
        if (mouseX >= rightPanelX && mouseX < carouselCenterX - 30)
        {
            prevSkin();
            return;
        }
        if (mouseX > carouselCenterX + 30 && mouseX <= rightPanelX + rightPanelWidth)
        {
            nextSkin();
            return;
        }
        if (mouseX >= carouselCenterX - 30 && mouseX <= carouselCenterX + 30)
        {
            selectAndConfirm();
            return;
        }
    }

    // Check click on nameplate
    if (mouseY >= nameplateY && mouseY <= nameplateY + nameplateHeight &&
        mouseX >= rightPanelX && mouseX <= rightPanelX + rightPanelWidth)
    {
        selectAndConfirm();
        return;
    }
}

void GuiSkinSelector::actionPerformed(GuiButton *button)
{
    if (!button->enabled)
        return;

    if (button->id == BUTTON_ID_LOAD_SKINS)
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
    drawRect(left - 1, top - 1, right + 1, bottom + 1, 0xFF181818);
    drawRect(left + 2, top + 2, right - 2, bottom - 2, fillColor);
    drawRect(left + 1, top + 1, right - 1, top + 2, 0xFFFFFFFF);
    drawRect(left + 1, top + 2, left + 2, bottom - 1, 0xFFFFFFFF);
    drawRect(left + 2, bottom - 2, right - 1, bottom - 1, 0xFF7A7A7A);
    drawRect(right - 2, top + 2, right - 1, bottom - 2, 0xFF7A7A7A);
}

void GuiSkinSelector::drawInsetPanel(int_t left, int_t top, int_t right, int_t bottom, int_t fillColor)
{
    drawRect(left, top, right, bottom, 0xFF222222);
    drawRect(left + 2, top + 2, right - 2, bottom - 2, fillColor);
    drawRect(left + 1, top + 1, right - 1, top + 2, 0xFF666666);
    drawRect(left + 1, top + 2, left + 2, bottom - 1, 0xFF666666);
    drawRect(left + 2, bottom - 2, right - 1, bottom - 1, 0xFFE8E8E8);
    drawRect(right - 2, top + 2, right - 1, bottom - 2, 0xFFE8E8E8);
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
    drawRect(x1, y1 + 1, x2, y2 - 1, color);

    renderEnable(RenderCapability::Texture2D);
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

// ----------------------------------------------------------------------------
// Screen Rendering
// ----------------------------------------------------------------------------

void GuiSkinSelector::drawScreen(int_t mouseX, int_t mouseY, float_t partialTick)
{
    (void)partialTick;

    // 1. Darkened background
    drawDefaultBackground();

    const int_t panelColor = 0xFFC6C6C6;

    // 2. Left Panel (Skin Pack Tabs)
    const int_t leftX1 = dialogLeft;
    const int_t leftY1 = dialogTop;
    const int_t leftX2 = dialogLeft + leftPanelWidth;
    const int_t leftY2 = dialogTop + dialogHeight;
    drawBeveledPanel(leftX1, leftY1, leftX2, leftY2, panelColor);

    // Pack artwork container (square)
    const int_t artPadding = 8;
    const int_t artSize = std::min<int_t>(leftPanelWidth - artPadding * 2, 70);
    const int_t artX = leftX1 + (leftPanelWidth - artSize) / 2;
    const int_t artY = leftY1 + 10;
    drawInsetPanel(artX - 2, artY - 2, artX + artSize + 2, artY + artSize + 2, 0xFF101010);

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

    // Tab 1: "Default Skins"
    const int_t packBtnX = leftX1 + 6;
    const int_t packBtnW = leftPanelWidth - 12;

    int defaultFill = (currentPackIndex == 0) ? 0xFF505050 : 0xFF9E9E9E;
    drawInsetPanel(packBtnX, tabDefaultTop, packBtnX + packBtnW, tabDefaultBottom, defaultFill);
    if (currentPackIndex == 0)
        drawRect(packBtnX - 1, tabDefaultTop - 1, packBtnX + packBtnW + 1, tabDefaultBottom + 1, 0xFFFFFF00);
    drawCenteredString(fontRenderer, "Default Skins", packBtnX + packBtnW / 2, tabDefaultTop + 7, 0xFFFFFF);

    // Tab 2: "Custom Skins" (Only drawn if custom skins exist)
    if (SkinManager::getPackCount() > 1)
    {
        int customFill = (currentPackIndex == 1) ? 0xFF505050 : 0xFF9E9E9E;
        drawInsetPanel(packBtnX, tabCustomTop, packBtnX + packBtnW, tabCustomBottom, customFill);
        if (currentPackIndex == 1)
            drawRect(packBtnX - 1, tabCustomTop - 1, packBtnX + packBtnW + 1, tabCustomBottom + 1, 0xFFFFFF00);

        std::string label = "Custom Skins (" + std::to_string(SkinManager::getSkinCountForPack(1)) + ")";
        drawCenteredString(fontRenderer, label, packBtnX + packBtnW / 2, tabCustomTop + 7, 0xFFFFFF);

        // Navigation hint at bottom of left panel
        fontRenderer->drawString("L1/R1: Tab", leftX1 + 8, leftY2 - 16, 0x555555);
    }

    // 3. Right Panel (Skin Carousel and Details)
    const int_t rightX1 = rightPanelX;
    const int_t rightY1 = dialogTop;
    const int_t rightX2 = rightPanelX + rightPanelWidth;
    const int_t rightY2 = dialogTop + dialogHeight;
    drawBeveledPanel(rightX1, rightY1, rightX2, rightY2, panelColor);

    // Top Header Banner in Right Panel
    const int_t headerH = 18;
    const int_t headerY = rightY1 + 5;
    drawRect(rightX1 + 4, headerY, rightX2 - 4, headerY + headerH, 0x88242424);
    fontRenderer->drawStringWithShadow(SkinManager::getPackName(currentPackIndex), rightX1 + 14, headerY + 5, 0xFFFFFF);

    // 4. Infinite Carousel Area
    const int totalSkins = SkinManager::getSkinCountForPack(currentPackIndex);
    if (totalSkins > 0)
    {
        const float spacing = static_cast<float>(rightPanelWidth) * 0.22f;

        const int slotOrder[] = { -3, 3, -2, 2, -1, 1, 0 };
        for (int k : slotOrder)
        {
            float t = static_cast<float>(k) - scrollOffset;
            float dist = std::fabs(t);
            if (dist > 2.6f)
                continue;

            int skinIndex = ((currentSkinIndex + k) % totalSkins + totalSkins) % totalSkins;
            const SkinEntry *skin = SkinManager::getSkin(currentPackIndex, skinIndex);
            if (skin == nullptr)
                continue;

            float scale = 3.4f - 1.05f * std::min(dist, 2.0f);
            if (scale < 1.0f) scale = 1.0f;

            float alpha = 1.0f - 0.20f * std::min(dist, 2.0f);
            if (alpha < 0.35f) alpha = 0.35f;

            float skinW = 16.0f * scale;
            float skinH = 32.0f * scale;
            float skinX = static_cast<float>(carouselCenterX) + t * spacing - skinW * 0.5f;
            float skinY = static_cast<float>(carouselGroundY) - skinH;

            // Draw soft ground shadow
            drawFeetShadow(static_cast<float>(carouselCenterX) + t * spacing, static_cast<float>(carouselGroundY),
                           skinW * 0.45f, 4.0f, alpha);

            // Draw front preview
            drawFrontPreview(skin, skinX, skinY, skinW, skinH, alpha);
        }
    }

    // 5. Bottom Nameplate Banner
    const int_t npPadding = 8;
    const int_t npW = rightPanelWidth - 44;
    const int_t npX = rightX1 + npPadding;
    const int_t npY = nameplateY;
    drawInsetPanel(npX, npY, npX + npW, npY + nameplateHeight, 0xFF383838);

    // Selected skin name
    const SkinEntry *selectedSkin = SkinManager::getSkin(currentPackIndex, currentSkinIndex);
    if (selectedSkin != nullptr)
    {
        std::string displayName = toUpperString(selectedSkin->name);
        drawCenteredString(fontRenderer, displayName, npX + npW / 2, npY + 10, 0xFFFFFF);
    }

    // Two small decorative indicator slots to the right of the nameplate
    const int_t slotX = npX + npW + 6;
    const int_t slotW = 14;
    const int_t slotH = 11;
    drawInsetPanel(slotX, npY, slotX + slotW, npY + slotH, 0xFF383838);
    drawInsetPanel(slotX, npY + 14, slotX + slotW, npY + 14 + slotH, 0xFF383838);

    // 6. Navigation arrows
    fontRenderer->drawStringWithShadow("<", rightX1 + 10, carouselGroundY - 48, 0xEEEEEE);
    fontRenderer->drawStringWithShadow(">", rightX2 - 16, carouselGroundY - 48, 0xEEEEEE);

    // 7. Footer / Controller Legend
    const int_t footerY = height - 14;
#if PLATFORM_PS2
    std::string hint = "[X] Select   [O] Back   [/\\ ] Load";
    if (currentPackIndex == 1)
        hint += "   [ ] Delete";
#elif PLATFORM_WII
    std::string hint = "[A] Select   [B] Back   [D-Pad] Navigate";
#else
    std::string hint = "[Enter] Select   [Esc] Back   [Del] Delete";
#endif
    fontRenderer->drawStringWithShadow(hint, dialogLeft + 100, footerY, 0xC0C0C0);

    // 8. Draw standard GUI controls
    GuiScreen::drawScreen(mouseX, mouseY, partialTick);
}
