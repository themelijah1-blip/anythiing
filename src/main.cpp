#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <map>
#include <random>
#include <set>

using namespace geode::prelude;

// NOTE: IDs below are PLACEHOLDERS. Use "select deco objects, then press Decorate" instead.
struct Theme {
    std::vector<std::string> keywords;
    std::vector<int> ids;
    float density;
    float minScale, maxScale;
    bool randomRot;
};

static const std::map<std::string, Theme> THEMES = {
    {"forest", {{"forest", "nature", "jungle", "tree"}, {1, 2, 3}, 0.5f, 0.6f, 1.6f, true}},
    {"space",  {{"space", "galaxy", "star", "cosmic"},  {1, 2, 3}, 0.3f, 0.3f, 1.2f, true}},
    {"neon",   {{"neon", "cyber", "glow", "synth"},     {1, 2, 3}, 0.6f, 0.5f, 1.4f, false}},
    {"spooky", {{"spooky", "halloween", "haunted", "dark"}, {1, 2, 3}, 0.4f, 0.6f, 1.5f, true}},
    {"candy",  {{"candy", "sweet", "cute", "pastel"},   {1, 2, 3}, 0.5f, 0.7f, 1.3f, true}},
    {"fire",   {{"fire", "lava", "hell", "demon"},      {1, 2, 3}, 0.5f, 0.6f, 1.6f, true}},
};

static float rand01(std::mt19937& rng) {
    return std::uniform_real_distribution<float>(0.f, 1.f)(rng);
}

static void decorate(
    LevelEditorLayer* lel, std::vector<int> const& palette, CCRect area,
    Theme const& t, int maxObjs, std::set<GameObject*> const& exclude
) {
    std::mt19937 rng{std::random_device{}()};

    std::vector<CCPoint> anchors;
    for (auto obj : CCArrayExt<GameObject*>(lel->m_objects)) {
        if (exclude.count(obj)) continue;
        auto p = obj->getPosition();
        if (area.containsPoint(p)) anchors.push_back(p);
    }

    int placed = 0;
    auto place = [&](CCPoint pos) {
        if (placed >= maxObjs) return;
        int id = palette[rng() % palette.size()];
        auto obj = lel->createObject(id, pos, true);
        if (!obj) return;
        float s = t.minScale + rand01(rng) * (t.maxScale - t.minScale);
        obj->setScale(s);
        if (t.randomRot) obj->setRotation(rand01(rng) * 360.f);
        placed++;
    };

    for (auto& a : anchors) {
        if (rand01(rng) > t.density) continue;
        float side = rand01(rng) < 0.5f ? -1.f : 1.f;
        float dx = (rand01(rng) - 0.5f) * 60.f;
        float dy = side * (25.f + rand01(rng) * 50.f);
        place(ccp(a.x + dx, a.y + dy));
    }

    int sprinkle = maxObjs / 5;
    for (int i = 0; i < sprinkle; i++) {
        place(ccp(
            area.getMinX() + rand01(rng) * area.size.width,
            area.getMinY() + rand01(rng) * area.size.height
        ));
    }

    Notification::create(fmt::format("Placed {} objects", placed), NotificationIcon::Success)->show();
}

class DecoratePopup : public geode::Popup {
protected:
    EditorUI* m_ui = nullptr;
    TextInput* m_input = nullptr;

    bool init(EditorUI* ui) {
        if (!Popup::init(380.f, 160.f)) return false;
        m_ui = ui;
        this->setTitle("Auto Decorator");

        m_input = TextInput::create(300.f, "theme: forest, space, neon, spooky, candy, fire");
        m_input->setPosition(m_mainLayer->getContentSize() / 2 + ccp(0, 10));
        m_mainLayer->addChild(m_input);

        auto btn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Decorate"), this, menu_selector(DecoratePopup::onGo));
        m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, ccp(0, 30));
        return true;
    }

    void onGo(CCObject*) {
        auto lel = m_ui->m_editorLayer;
        int maxObjs = (int)Mod::get()->getSettingValue<int64_t>("max-objects");

        auto cam = lel->m_objectLayer->getPosition();
        auto win = CCDirector::get()->getWinSize();
        CCRect area(-cam.x, -cam.y, win.width, win.height);

        std::vector<int> palette;
        std::set<GameObject*> exclude;
        Theme t{{}, {}, 0.4f, 0.6f, 1.4f, true};

        if (auto sel = m_ui->getSelectedObjects()) {
            for (auto obj : CCArrayExt<GameObject*>(sel)) {
                palette.push_back(obj->m_objectID);
                exclude.insert(obj);
            }
        }

        if (palette.empty()) {
            std::string prompt = m_input->getString();
            std::transform(prompt.begin(), prompt.end(), prompt.begin(), ::tolower);
            bool matched = false;
            for (auto& [name, theme] : THEMES) {
                for (auto& kw : theme.keywords) {
                    if (prompt.find(kw) == std::string::npos) continue;
                    if (!matched) t = theme;
                    palette.insert(palette.end(), theme.ids.begin(), theme.ids.end());
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                FLAlertLayer::create(
                    "No theme matched",
                    "Try: forest, space, neon, spooky, candy, fire.\n"
                    "Or select some decoration objects first and press Decorate.",
                    "OK")->show();
                return;
            }
        }

        decorate(lel, palette, area, t, maxObjs, exclude);
        this->onClose(nullptr);
    }

public:
    static DecoratePopup* create(EditorUI* ui) {
        auto ret = new DecoratePopup();
        if (ret->init(ui)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(DecoratorEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        auto btn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("AI", "bigFont.fnt", "GJ_button_01.png", 0.6f),
            this, menu_selector(DecoratorEditorUI::onDecorate));
        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition(CCDirector::get()->getWinSize().width - 40.f, 40.f);
        this->addChild(menu);
        return true;
    }

    void onDecorate(CCObject*) { DecoratePopup::create(this)->show(); }
};
