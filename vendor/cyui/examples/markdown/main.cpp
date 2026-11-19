#include <cyui/modules/DocumentEditor.h>
#include <cyui/modules/DocumentIO.h>
#include "Platform.h"
#include "Samples.h"
#include "Assets.h"
#include <imgui_internal.h>
#include <SDL3/SDL_main.h>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
using namespace cyui;
namespace fs = std::filesystem;

void translations() {
    registerTranslations("fr",
                         {{"Markdown playground", "Laboratoire Markdown"},
                          {"Chat", "Tchat"},
                          {"Markdown document", "Document Markdown"},
                          {"Local conversation", "Conversation locale"},
                          {"Sample conversation", "Conversation d’exemple"},
                          {"Clear conversation", "Vider la conversation"},
                          {"Simulate reply", "Simuler une réponse"},
                          {"Stop reply", "Arrêter la réponse"},
                          {"Local sample, no AI service", "Exemple local, sans service d’IA"},
                          {"Follow new messages", "Suivre les nouveaux messages"},
                          {"You", "Vous"},
                          {"Assistant", "Assistant"},
                          {"Message role", "Rôle du message"},
                          {"Preview draft", "Aperçu du brouillon"},
                          {"Send message (Ctrl+Enter)", "Envoyer (Ctrl+Entrée)"},
                          {"Type Markdown below. Enter inserts a new line.",
                           "Saisissez du Markdown ci-dessous. Entrée ajoute une ligne."},
                          {"Copy last message to document", "Ouvrir le dernier message dans le document"},
                          {"Send document to chat", "Envoyer le document dans le tchat"},
                          {"Load sample document", "Charger le document d’exemple"},
                          {"Open Markdown (Ctrl+O)", "Ouvrir un Markdown (Ctrl+O)"},
                          {"Save Markdown (Ctrl+S)", "Enregistrer le Markdown (Ctrl+S)"},
                          {"Save Markdown as...", "Enregistrer le Markdown sous…"},
                          {"Copy Markdown", "Copier le Markdown"},
                          {"Read only", "Lecture seule"},
                          {"Unsaved document", "Document non enregistré"},
                          {"Modified", "Modifié"},
                          {"Saved", "Enregistré"},
                          {"Sample", "Exemple"},
                          {"Unable to open this file", "Impossible d’ouvrir ce fichier"},
                          {"Unable to save this file", "Impossible d’enregistrer ce fichier"},
                          {"File is too large (16 MiB maximum)", "Fichier trop volumineux (16 Mio maximum)"},
                          {"Save document changes before continuing?",
                           "Enregistrer les modifications du document avant de continuer ?"},
                          {"Copied", "Copié"},
                          {"Link copied", "Lien copié"},
                          {"No messages yet", "Aucun message"},
                          {"Write a message or load the sample conversation.",
                           "Écrivez un message ou chargez la conversation d’exemple."},
                          {"%zu messages", "%zu messages"},
                          {"%zu bytes | %zu lines", "%zu octets | %zu lignes"}});
}

bool hasText(const std::string &text) {
    return text.find_first_not_of(" \t\r\n") != std::string::npos;
}

std::string now() {
    SDL_Time ticks{};
    SDL_DateTime time{};
    SDL_GetCurrentTime(&ticks);
    SDL_TimeToDateTime(ticks, &time, true);
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", time.hour, time.minute);
    return buffer;
}
void link(const std::string &url) {
    if (url.empty())
        return;
    if (url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0 || url.rfind("mailto:", 0) == 0)
        SDL_OpenURL(url.c_str());
    else {
        ImGui::SetClipboardText(url.c_str());
        notify(tr("Link copied").c_str());
    }
}

struct App {
    platform::Host &host;
    platform::Dialogs dialogs;
    DocumentEditor document;
    MessageComposerState composer, editingDraft;
    TranscriptState conversation;
    bool virtualHistory = false;
    TranscriptMessage virtualRow;
    std::vector<TranscriptMessage> messages;
    std::vector<TranscriptAttachment> pending;
    ItemId nextId = 1, editing = 0, replying = 0;
    int page = 0;
    bool quit = false, readOnly = false;
    fs::path path;
    std::string savedMarkdown;
    std::uint64_t dirtyRevision = ~std::uint64_t(0), preparedRevision = ~std::uint64_t(0);
    bool modified = false;
    native_assets::Store assets;
    DocumentServices services;
    std::string sampleImage;

    explicit App(platform::Host &owner) : host(owner), assets(owner.renderer) {
        translations();
        registerTranslations(
            "fr",
            {{"Attach files", "Joindre des fichiers"},
             {"Save attachment", "Enregistrer la pièce jointe"},
             {"Reply", "Répondre"},
             {"Edit message", "Modifier le message"},
             {"Delete message", "Supprimer le message"},
             {"Save message (Enter)", "Enregistrer (Entrée)"},
             {"Send message (Enter)", "Envoyer (Entrée)"},
             {"Message…", "Écrire un message…"},
             {"(edited)", "(modifié)"},
             {"Enter to send · Shift+Enter for a new line",
              "Entrée pour envoyer · Maj+Entrée pour une nouvelle ligne"},
             {"Escape to cancel · Enter to save", "Échap pour annuler · Entrée pour enregistrer"},
             {"Insert files or images", "Insérer des fichiers ou des images"},
             {"Replying to", "Réponse à"},
             {"Remove attachment", "Retirer la pièce jointe"},
             {"New document", "Nouveau document"},
             {"Local test channel", "Salon de test local"},
             {"Unable to import file (64 MiB maximum)", "Impossible d’importer ce fichier (64 Mio maximum)"},
             {"Unable to read attachment", "Impossible de lire la pièce jointe"},
             {"Unable to save attachment", "Impossible d’enregistrer la pièce jointe"},
             {"Copy code", "Copier le code"}});
        services.image = [&](const std::string &source) { return assets.image(source); };
        sampleImage = assets.sample().uri;
        document.setMarkdown(sampleMarkdown());
        document.setMode(DocumentMode::live);
        savedMarkdown = document.markdown();
        seedChat();
    }
    std::string sampleMarkdown() const {
        auto text = std::string(samples::document);
        text.replace(text.find("sample://palette"), std::string("sample://palette").size(), sampleImage);
        return text;
    }
    bool dirty() {
        const auto &snapshot = document.document();
        if (dirtyRevision != snapshot.revision()) {
            modified = snapshot.markdown() != savedMarkdown;
            dirtyRevision = snapshot.revision();
        }
        return modified;
    }
    void setDocument(const std::string &text, fs::path file, bool saved) {
        document.setMarkdown(text);
        std::unordered_set<ItemId> keep;
        for (const auto &message : messages) {
            assets.retainReferences(message.content, keep);
            for (const auto &attached : message.attachments)
                keep.insert(attached.id);
        }
        for (const auto &attached : pending)
            keep.insert(attached.id);
        for (const auto *source : {&sampleImage, &composer.text, &editingDraft.text})
            assets.retainReferences(*source, keep);
        assets.retainReferences(text, keep);
        assets.resetDocument(keep);
        path = std::move(file);
        savedMarkdown = saved ? text : "";
        dirtyRevision = preparedRevision = ~std::uint64_t(0);
    }
    void prepareDocument() {
        const auto &snapshot = document.document();
        if (preparedRevision != snapshot.revision()) {
            assets.prepare(snapshot, path.empty() ? fs::current_path() : path.parent_path());
            preparedRevision = snapshot.revision();
        }
    }
    void addMessage(int speaker, const std::string &content,
                    std::vector<TranscriptAttachment> attachments = {}, ItemId reply = 0) {
        TranscriptMessage message{nextId++, speaker ? "Maya" : tr("You"), content, now()};
        message.attachments = std::move(attachments);
        message.own = speaker == 0;
        message.replyTo = reply;
        messages.push_back(std::move(message));
    }
    void clearChat() {
        messages.clear();
        conversation = {};
        editing = replying = 0;
    }
    void seedChat() {
        clearChat();
        addMessage(1, "Salut ! Ici, on teste le **Markdown**, les images et les pièces jointes.");
        const auto *image = assets.find(sampleImage);
        addMessage(0, "Une image native, que tu peux ouvrir en cliquant dessus.", {image->attachment()});
        const std::string note = "# Notes du projet\n\n- [ ] Tester l’édition\n- [x] Ajouter des images\n";
        auto &file = assets.add("notes.md", {note.begin(), note.end()});
        addMessage(1, "Voici aussi un document joint.\n\n> Le clic ouvre le document dans l’autre vue.",
                   {file.attachment()});
        addMessage(0, "Essaie de survoler ce message, ou de faire un clic droit.\n\n`Entrée` envoie et "
                      "`Maj+Entrée` ajoute une ligne.");
    }
    bool send() {
        if (!hasText(composer.text) && pending.empty())
            return false;
        addMessage(0, composer.text, pending, replying);
        composer.text.clear();
        composer.first = composer.last = 0;
        composer.focus = true;
        pending.clear();
        replying = 0;
        conversation.jumpToBottom = true;
        return true;
    }
    void beginEdit(ItemId id) {
        for (const auto &message : messages)
            if (message.id == id && message.own) {
                editing = id;
                editingDraft.text = message.content;
                editingDraft.focus = true;
                conversation.scrollTo = id;
            }
    }
    bool finishEdit(bool cancel = false) {
        if (!editing)
            return false;
        for (auto &message : messages)
            if (message.id == editing) {
                if (!cancel && !hasText(editingDraft.text) && message.attachments.empty())
                    return false;
                if (!cancel && message.content != editingDraft.text) {
                    message.content = editingDraft.text;
                    message.edited = true;
                    conversation.heights.erase(message.id);
                }
            }
        editing = 0;
        composer.focus = true;
        return true;
    }
    void chooseAttachments(bool chat) {
        dialogs.choose(host.window, false, true, false, "",
                       [this, chat](const auto &files) { importFiles(files, chat); });
    }
    void importFiles(const std::vector<fs::path> &files, bool chat) {
        for (const auto &file : files) {
            try {
                auto &asset = assets.import(file);
                if (chat)
                    pending.push_back(asset.attachment());
                else
                    document.insert("\n\n" + assets.markup(asset) + "\n\n");
            } catch (const std::exception &e) {
                notify(tr(e.what()).c_str(), Tone::error);
            }
        }
        if (chat)
            composer.focus = true;
    }
    bool paste(bool chat, std::string &replacement) {
        auto data = platform::clipboard();
        if (!data.files.empty()) {
            importFiles(data.files, chat);
            return true;
        }
        if (!data.image.empty()) {
            auto &image = assets.add(data.imageName, std::move(data.image));
            if (image.image.texture) {
                if (chat)
                    pending.push_back(image.attachment());
                else
                    replacement = "\n\n" + assets.markup(image) + "\n\n";
                return true;
            }
        }
        replacement = data.html.empty() ? "" : markdownFromHtml(data.html);
        return !replacement.empty();
    }
    void load(const fs::path &file) {
        const auto bytes = native_assets::read(file);
        if (bytes.size() > 16 * 1024 * 1024)
            throw std::runtime_error("File is too large (16 MiB maximum)");
        setDocument(decodeMarkdown({bytes.data(), bytes.size()}).markdown, fs::absolute(file), true);
        page = 1;
    }
    bool saveTo(const fs::path &file) {
        std::string text;
        try {
            prepareDocument();
            text = assets.exportMarkdown(document.document(), fs::absolute(file));
        } catch (...) {
            return false;
        }
        if (!platform::writeFile(file, text.data(), text.size()))
            return false;
        path = fs::absolute(file);
        savedMarkdown = document.markdown();
        dirtyRevision = ~std::uint64_t(0);
        return true;
    }
    void save(bool as = false, std::function<void()> after = {}) {
        const auto write = [this, after](fs::path file) {
            if (file.empty())
                return;
            if (file.extension().empty())
                file += ".md";
            if (!saveTo(file)) {
                notify(tr("Unable to save this file").c_str(), Tone::error);
                return;
            }
            notify(tr("Saved").c_str(), Tone::success);
            if (after)
                after();
        };
        if (!as && !path.empty())
            write(path);
        else
            dialogs.choose(host.window, true, false, true, path.empty() ? "notes.md" : platform::utf8(path),
                           [write](const auto &files) {
                               if (!files.empty())
                                   write(files.front());
                           });
    }
    void replace(std::function<void()> after) {
        if (!dirty()) {
            after();
            return;
        }
        const auto title = tr("Markdown playground");
        const auto prompt = tr("Save document changes before continuing?");
        const auto cancel = tr("Cancel"), discard = tr("Discard"), saveLabel = tr("Save");
        const SDL_MessageBoxButtonData buttons[]{
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, cancel.c_str()},
            {0, 1, discard.c_str()},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 2, saveLabel.c_str()}};
        const SDL_MessageBoxData box{
            SDL_MESSAGEBOX_WARNING, host.window, title.c_str(), prompt.c_str(), 3, buttons, nullptr};
        int selected = 0;
        if (!SDL_ShowMessageBox(&box, &selected))
            return;
        if (selected == 1)
            after();
        else if (selected == 2)
            save(false, std::move(after));
    }
    void open() {
        dialogs.choose(host.window, false, false, true, "", [this](const auto &files) {
            if (files.empty())
                return;
            const auto file = files.front();
            replace([this, file] {
                try {
                    load(file);
                } catch (const std::exception &e) {
                    notify(tr(e.what()).c_str(), Tone::error);
                }
            });
        });
    }
    void toDocument(const std::string &text) {
        replace([this, text] {
            setDocument(text, {}, false);
            page = 1;
        });
    }
    void saveAttachment(const native_assets::Asset &asset) {
        const auto source = assets.retain(asset.uri);
        dialogs.choose(host.window, true, false, false, asset.name, [source](const auto &files) {
            if (files.empty())
                return;
            const bool ok = platform::writeFile(files.front(), source->bytes.data(), source->bytes.size());
            notify(tr(ok ? "Saved" : "Unable to save attachment").c_str(), ok ? Tone::success : Tone::error);
        });
    }
    void activate(const std::string &source, bool download = false) {
        if (const auto *asset = assets.find(source)) {
            auto ext = platform::utf8(platform::path(asset->name).extension());
            for (auto &c : ext)
                if (c >= 'A' && c <= 'Z')
                    c += 'a' - 'A';
            if (!download && (ext == ".md" || ext == ".markdown" || ext == ".txt")) {
                try {
                    toDocument(decodeMarkdown({asset->bytes.data(), asset->bytes.size()}).markdown);
                } catch (...) {
                    saveAttachment(*asset);
                }
            } else
                saveAttachment(*asset);
        } else
            link(source);
    }
    void preferences() {
        ImGui::SetNextItemWidth(dp(150));
        const auto &themes = builtInThemes();
        std::string selected;
        for (const auto &theme : themes)
            if (theme.key == currentThemeKey())
                selected = tr(theme.name.c_str());
        if (ImGui::BeginCombo("##theme", selected.c_str())) {
            for (const auto &theme : themes)
                if (ImGui::Selectable(tr(theme.name.c_str()).c_str(), theme.key == currentThemeKey()))
                    setTheme(theme.key.c_str());
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        bool comfortable = density() == Density::comfortable;
        if (ImGui::Checkbox(trLabel("Comfortable").c_str(), &comfortable))
            setDensity(comfortable ? Density::comfortable : Density::compact);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(dp(100));
        if (ImGui::BeginCombo("##language", locale() == "fr" ? "Français" : "English")) {
            for (const auto &lang : availableLocales())
                if (ImGui::Selectable(lang.name.c_str(), lang.code == locale())) {
                    setLocale(lang.code.c_str());
                    for (auto &message : messages)
                        if (message.own)
                            message.role = tr("You");
                }
            ImGui::EndCombo();
        }
    }
    void chatView() {
        {
            ScopedFont bold(boldFont());
            ImGui::TextUnformatted("# markdown");
        }
        ImGui::SameLine();
        textFaint("%s", tr("Local test channel").c_str());
        ImGui::SameLine();
        alignRight(ImGui::GetFrameHeight());
        if (iconButton(icon::more, tr("More").c_str()))
            ImGui::OpenPopup("##chatOptions");
        if (ImGui::BeginPopup("##chatOptions")) {
            if (ImGui::MenuItem(trLabel("Sample conversation").c_str()))
                seedChat();
            if (ImGui::MenuItem(trLabel("Clear conversation").c_str()))
                clearChat();
            ImGui::EndPopup();
        }
        ImGui::Separator();
        const float inputWidth = std::max(dp(80), ImGui::GetContentRegionAvail().x - dp(90));
        const auto measured = ImGui::CalcTextSize(composer.text.c_str(), nullptr, false, inputWidth);
        const float composerHeight = std::clamp(measured.y + ImGui::GetStyle().FramePadding.y * 2 + dp(8),
                                                ImGui::GetFrameHeight() + dp(8), dp(180));
        const float reserve = composerHeight + ImGui::GetTextLineHeightWithSpacing() * 2 + dp(16) +
                              (pending.empty() ? 0 : dp(110)) +
                              (replying ? ImGui::GetFrameHeightWithSpacing() : 0);
        TranscriptOptions options;
        options.document.services = &services;
        options.document.tasks = false;
        options.document.linkPreviews = false;
        options.editing = editing;
        options.editor = [&](const TranscriptMessage &message, float width) {
            MessageComposerOptions edit;
            edit.attachments = false;
            edit.editing = true;
            edit.canSubmitEmpty = !message.attachments.empty();
            edit.paste = [&](std::string &text) {
                text = markdownFromHtml(platform::clipboard().html);
                return !text.empty();
            };
            const auto action = messageComposer("edit", editingDraft, {width, 0}, edit);
            textFaint("%s", tr("Escape to cancel · Enter to save").c_str());
            if (action.cancelled)
                finishEdit(true);
            else if (action.submitted)
                finishEdit();
        };
        const float historyHeight = std::max(dp(80), ImGui::GetContentRegionAvail().y - reserve);
        if (messages.empty()) {
            ImGui::BeginChild("##emptyChat", {0, historyHeight});
            emptyState(tr("No messages yet").c_str(),
                       tr("Write a message or load the sample conversation.").c_str(), icon::mail);
            ImGui::EndChild();
        } else {
            TranscriptSource history;
            history.id = 1;
            history.count = 1'000'000;
            history.message = [&](std::size_t index) -> const TranscriptMessage * {
                virtualRow = {ItemId(index + 1), tr("User"),
                              "**Markdown** #" + std::to_string(index + 1) +
                                  (index % 3 == 0 ? "\nDeuxième ligne · UTF-8 : été, cœur." : ""),
                              "12:00"};
                virtualRow.own = false;
                if (index % 17 == 0)
                    virtualRow.attachments.push_back(assets.find(sampleImage)->attachment());
                return &virtualRow;
            };
            history.indexOf = [](ItemId id) { return id ? std::size_t(id - 1) : std::string::npos; };
            const auto action = virtualHistory
                                    ? transcript("chat", conversation, history, {0, historyHeight}, options)
                                    : transcript("chat", conversation, {messages.data(), messages.size()},
                                                 {0, historyHeight}, options);
            activate(action.link);
            if (action.edit)
                beginEdit(action.edit);
            if (action.reply) {
                replying = action.reply;
                composer.focus = true;
            }
            if (action.attachmentMessage)
                for (const auto &m : messages)
                    if (m.id == action.attachmentMessage && action.attachmentIndex < m.attachments.size())
                        activate(m.attachments[action.attachmentIndex].source, action.download);
            if (action.erase) {
                messages.erase(std::remove_if(messages.begin(), messages.end(),
                                              [&](const auto &m) { return m.id == action.erase; }),
                               messages.end());
                if (editing == action.erase)
                    editing = 0;
                if (replying == action.erase)
                    replying = 0;
            }
        }
        if (replying)
            for (const auto &message : messages)
                if (message.id == replying) {
                    textDim("%s %s", tr("Replying to").c_str(), message.role.c_str());
                    ImGui::SameLine();
                    if (iconButton(icon::close, tr("Cancel").c_str()))
                        replying = 0;
                }
        if (!pending.empty()) {
            ImGui::BeginChild("##pending", {0, dp(110)}, ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_HorizontalScrollbar);
            std::size_t remove = std::string::npos;
            for (std::size_t n = 0; n < pending.size(); ++n) {
                if (n)
                    ImGui::SameLine();
                ScopedID key{int(n)};
                ImGui::BeginGroup();
                const auto image = assets.image(pending[n].source);
                if (image.texture)
                    ImGui::Image(image.texture, {dp(90), dp(56)});
                else {
                    ScopedFont font(boldFont());
                    ImGui::TextUnformatted("FILE");
                }
                textDim("%s", pending[n].name.c_str());
                ImGui::SameLine();
                if (iconButton(icon::close, tr("Remove attachment").c_str()))
                    remove = n;
                ImGui::EndGroup();
            }
            ImGui::EndChild();
            if (remove != std::string::npos)
                pending.erase(pending.begin() + std::ptrdiff_t(remove));
        }
        MessageComposerOptions input;
        input.canSubmitEmpty = !pending.empty();
        input.paste = [&](std::string &replacement) { return paste(true, replacement); };
        if (composer.input && GImGui->ActiveId == composer.input && composer.text.empty() &&
            ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
            for (auto it = messages.rbegin(); it != messages.rend(); ++it)
                if (it->own) {
                    beginEdit(it->id);
                    break;
                }
        }
        const auto action = messageComposer("composer", composer, {0, composerHeight}, input);
        if (action.attach)
            chooseAttachments(true);
        if (action.submitted)
            send();
        if (action.cancelled)
            replying = 0;
        textFaint("%s", tr("Enter to send · Shift+Enter for a new line").c_str());
    }
    void documentView() {
        if (iconButton(icon::document, tr("New document").c_str()))
            replace([this] {
                setDocument("", {}, true);
                document.focus();
            });
        ImGui::SameLine();
        if (iconButton(icon::folder, tr("Open Markdown (Ctrl+O)").c_str()))
            open();
        ImGui::SameLine();
        if (iconButton(icon::save, tr("Save Markdown (Ctrl+S)").c_str()))
            save();
        ImGui::SameLine();
        if (iconButton(icon::download, tr("Save Markdown as...").c_str()))
            save(true);
        ImGui::SameLine();
        if (iconButton(icon::photo, tr("Insert files or images").c_str()))
            chooseAttachments(false);
        ImGui::SameLine();
        if (iconButton(icon::copy, tr("Copy Markdown").c_str()))
            ImGui::SetClipboardText(document.markdown().c_str());
        ImGui::SameLine();
        if (iconButton(icon::send, tr("Send document to chat").c_str())) {
            prepareDocument();
            addMessage(0, assets.chatMarkdown(document.document()));
            page = 0;
            conversation.jumpToBottom = true;
        }
        ImGui::SameLine();
        if (iconButton(icon::refresh, tr("Load sample document").c_str()))
            replace([this] {
                setDocument(sampleMarkdown(), {}, true);
            });
        ImGui::SameLine();
        if (iconButton(icon::lock, tr("Read only").c_str(), readOnly))
            readOnly = !readOnly;
        const auto name = path.empty() ? tr("Unsaved document") : platform::utf8(path.filename());
        ImGui::SameLine();
        textDim("%s%s", name.c_str(), dirty() ? " *" : "");
        prepareDocument();
        DocumentEditorOptions edit;
        edit.readOnly = readOnly;
        edit.lineNumbers = false;
        edit.toolbar = document.mode() != DocumentMode::reading;
        edit.document.services = &services;
        edit.document.linkPreviews = false;
        edit.pasteRich = [&] {
            std::string text;
            paste(false, text);
            return text;
        };
        const auto result = document.draw("document", ImGui::GetContentRegionAvail(), edit);
        activate(result.document.link);
    }
    void frame() {
        if (host.closeRequested && !dialogs.busy()) {
            host.closeRequested = false;
            replace([this] { quit = true; });
        }
        if (!host.dropped.empty() && !dialogs.busy()) {
            std::vector<fs::path> files;
            for (const auto &name : host.dropped)
                files.emplace_back(name);
            host.dropped.clear();
            if (page == 1 && files.size() == 1 &&
                (files[0].extension() == ".md" || files[0].extension() == ".markdown")) {
                replace([this, file = files[0]] {
                    try {
                        load(file);
                    } catch (...) {
                        notify(tr("Unable to open this file").c_str(), Tone::error);
                    }
                });
            } else
                importFiles(files, page == 0);
        }
        const auto *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##markdownPlayground", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::BeginDisabled(dialogs.busy());
        const auto chat = tr("Chat"), md = tr("Markdown document");
        const char *pages[]{chat.c_str(), md.c_str()};
        segmented("##page", pages, 2, &page, dp(300));
        ImGui::SameLine();
        alignRight(dp(445));
        preferences();
        ImGui::Separator();
        if (page == 0)
            chatView();
        else
            documentView();
        if (!dialogs.busy() && (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) &&
            ImGui::IsKeyPressed(ImGuiKey_O, false))
            open();
        if (!dialogs.busy() && (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) &&
            ImGui::IsKeyPressed(ImGuiKey_S, false))
            save(ImGui::GetIO().KeyShift);
        ImGui::EndDisabled();
        ImGui::End();
        renderNotifications();
    }
};

bool selfTest(App &app) {
    const auto check = [](bool value, const char *what) {
        if (!value)
            std::fprintf(stderr, "FAIL: %s\n", what);
        return value;
    };
    app.clearChat();
    app.composer.text = " \n\t";
    if (!check(!app.send(), "reject empty messages"))
        return false;
    const auto *picture = app.assets.find(app.sampleImage);
    if (!check(picture && picture->image.texture && !picture->bytes.empty(),
               "portable PNG encode/decode and native texture"))
        return false;
    app.pending.push_back(picture->attachment());
    app.composer.text.clear();
    if (!check(app.send() && app.messages.back().attachments.size() == 1, "attachment-only message"))
        return false;
    app.composer.text = "**été / cœur**";
    if (!check(app.send() && app.composer.text.empty(), "UTF-8 message and composer reset"))
        return false;
    const auto message = app.messages.back().id;
    app.beginEdit(message);
    app.editingDraft.text = "**texte modifié**";
    if (!check(app.finishEdit() && app.messages.back().edited &&
                   app.messages.back().content == "**texte modifié**",
               "inline edit commit"))
        return false;
    app.beginEdit(message);
    app.editingDraft.text = "cancelled";
    app.finishEdit(true);
    if (!check(app.messages.back().content == "**texte modifié**", "Escape cancels edit"))
        return false;
    app.replying = message;
    app.composer.text = "réponse";
    app.send();
    if (!check(app.messages.back().replyTo == message, "reply reference"))
        return false;
    app.addMessage(1, "peer");
    app.beginEdit(app.messages.back().id);
    if (!check(app.editing == 0, "cannot edit another participant's message"))
        return false;
    const auto uris = platform::fileUris("copy\n# comment\nfile:///tmp/%C3%A9t%C3%A9%20notes.md\r\n"
                                         "file://server/share.txt\nhttps://example.org/file\n"
                                         "file:///tmp/bad%00path\nfile:///tmp/bad%zzpath");
    if (!check(uris.size() == 1 && platform::utf8(uris.front()).find("été notes.md") != std::string::npos,
               "file clipboard URI decoding preserves UTF-8 and rejects remote/malformed paths"))
        return false;
    if (!check(raster::decode(reinterpret_cast<const unsigned char *>("invalid"), 7).pixels.empty(),
               "invalid attachments stay files without creating a texture"))
        return false;
    const auto directory =
        fs::temp_directory_path() / ("cyui-markdown-" + std::to_string(SDL_GetPerformanceCounter()));
    if (!fs::create_directory(directory))
        return false;
    const auto file = directory / platform::path("été-notes.md");
    app.document.setMarkdown("# été\n\n![palette](" + app.sampleImage + ")\n");
    bool saved = app.saveTo(file);
    app.document.setMarkdown(app.document.markdown() + "\nUpdated: cœur.\n");
    saved = saved && app.saveTo(file);
    const auto written = native_assets::read(file);
    const bool protectedFile =
        !platform::writeFile(directory, "bad", 3) && native_assets::read(file) == written;
    try {
        app.load(file);
        app.prepareDocument();
    } catch (...) {
        return false;
    }
    Document loaded;
    loaded.setMarkdown(app.document.markdown());
    std::string imagePath;
    for (const auto &block : loaded.blocks())
        for (const auto &run : block.inlines)
            if (run.kind == DocumentInlineKind::image)
                imagePath = run.target;
    const bool imageLoaded = app.assets.image(imagePath).texture != 0;
    if (!imagePath.empty())
        fs::remove(directory / platform::path(imagePath));
    fs::remove(directory / "assets");
    fs::remove(file);
    fs::remove(directory);
    if (!check(saved && protectedFile && !app.dirty() && imageLoaded &&
                   app.document.markdown().find("Updated: cœur.") != std::string::npos &&
                   app.document.markdown().find("asset://") == std::string::npos,
               "plain Markdown round trip preserves real image attachments"))
        return false;
    // Inline destinations, shared definitions and literal text can use the exact same spelling.
    const auto sourceDir = directory / "source", targetDir = directory / "target";
    fs::create_directories(sourceDir);
    fs::create_directories(targetDir);
    const auto original = sourceDir / "source.md", exported = targetDir / "copy.md";
    const std::string prefix = "Inline code `(logo.png)` and a fence:\n\n```\n(logo.png)\n```\n\n";
    const auto withTarget = [&](const std::string &target) {
        return prefix + "![Logo](<" + target + "> \"Cover\")\n\n![first][ref] and ![second][ref]\n\n[ref]: " +
               target + " \"Title\"\n";
    };
    const auto fixture = withTarget("logo.png");
    if (!platform::writeFile(sourceDir / "logo.png", picture->bytes.data(), picture->bytes.size()) ||
        !platform::writeFile(original, fixture.data(), fixture.size()))
        return false;
    app.load(original);
    app.prepareDocument();
    Document chatCopy;
    chatCopy.setMarkdown(app.assets.chatMarkdown(app.document.document()));
    if (!check(chatCopy.destinations().size() == 3, "inline and reference images are parsed"))
        return false;
    auto oldUri = chatCopy.destinations().front().target;
    app.document.select(0, fixture.size());
    app.document.insert("");
    app.prepareDocument();
    app.document.command(DocumentCommand::undo);
    app.prepareDocument();
    if (!check(app.document.markdown() == fixture && app.assets.find(oldUri) &&
                   app.assets.image("logo.png").texture, "undo retains the original attachment and texture"))
        return false;
    app.load(original);
    if (!check(!app.assets.find(oldUri) && !app.document.canRedo(),
               "loading starts a new history and releases unreferenced bytes and textures"))
        return false;
    app.prepareDocument();
    chatCopy.setMarkdown(app.assets.chatMarkdown(app.document.document()));
    oldUri = chatCopy.destinations().front().target;
    app.addMessage(0, chatCopy.markdown());
    if (!app.saveTo(exported))
        return false;
    const auto copy = native_assets::read(exported);
    Document exportedDoc;
    exportedDoc.setMarkdown({copy.begin(), copy.end()});
    if (exportedDoc.destinations().empty())
        return false;
    const auto target = exportedDoc.destinations().front().target;
    if (!check(target.rfind("assets/", 0) == 0 && exportedDoc.markdown() == withTarget(target) &&
                   native_assets::read(targetDir / platform::path(target)) == picture->bytes,
               "Save As relocates image destinations while preserving code, titles and shared references"))
        return false;
    app.load(exported);
    if (!check(app.assets.find(oldUri) && app.assets.find(app.sampleImage),
               "document switching retains assets referenced by chat and sample"))
        return false;
    fs::remove(targetDir / platform::path(target));
    fs::remove(targetDir / "assets");
    fs::remove(exported);
    fs::remove(targetDir);
    fs::remove(original);
    fs::remove(sourceDir / "logo.png");
    fs::remove(sourceDir);
    fs::remove(directory);
    app.setDocument(app.sampleMarkdown(), {}, true);
    app.seedChat();
    app.composer.text.clear();
    return true;
}
} // namespace

int main(int count, char **args) {
    std::string capture, file;
    std::string theme = "mid-grey", language = "en", scenario;
    bool comfortable = false, test = false;
    int page = 0, mode = int(cyui::DocumentMode::live), captureFrames = 44;
    for (int n = 1; n < count; ++n) {
        const std::string arg = args[n];
        const auto next = [&]() { return n + 1 < count ? std::string(args[++n]) : std::string(); };
        if (arg == "--capture")
            capture = next();
        else if (arg == "--file")
            file = next();
        else if (arg == "--theme")
            theme = next();
        else if (arg == "--locale")
            language = next();
        else if (arg == "--scenario")
            scenario = next();
        else if (arg == "--frames")
            captureFrames = int(std::clamp(std::strtol(next().c_str(), nullptr, 10), 1L, 10000L));
        else if (arg == "--comfortable")
            comfortable = true;
        else if (arg == "--self-test")
            test = true;
        else if (arg == "--view")
            page = next() == "markdown" ? 1 : 0;
        else if (arg == "--document-mode") {
            const auto value = next();
            mode = value == "live" ? 0 : value == "source" ? 2 : value == "reading" ? 3 : 1;
        } else {
            return 2;
        }
    }
    const bool automated = test || !capture.empty();
    platform::Host host;
    if (!host.init("CyUI - Markdown [revision 7 - multiplatform]", automated)) {
        std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    const float scale = host.scale();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    cyui::Config config;
    config.themeKey = theme.c_str();
    config.dpiScale = scale;
    config.density = comfortable ? cyui::Density::comfortable : cyui::Density::compact;
    cyui::apply(config);
    translations();
    cyui::setLocale(language.c_str());
    cyui::loadFonts();
    if (!host.initImGui()) {
        std::fprintf(stderr, "SDL/ImGui: %s\n", SDL_GetError());
        return 1;
    }
    int exitCode = 0;
    {
        App app(host);
        app.page = page;
        app.document.setMode(cyui::DocumentMode(mode));
        if (!file.empty()) {
            try {
                app.load(platform::path(file));
            } catch (const std::exception &e) {
                cyui::notify(cyui::tr(e.what()).c_str(), cyui::Tone::error);
                exitCode = 1;
            }
        }
        if (scenario == "edit")
            app.beginEdit(app.messages.back().id);
        if (scenario == "reply")
            app.replying = app.messages.front().id;
        if (scenario == "pending") {
            app.pending.push_back(app.assets.find(app.sampleImage)->attachment());
            app.pending.push_back(app.messages[2].attachments.front());
        }
        if (scenario == "lightbox") {
            auto &view = app.conversation.views[app.messages[1].id];
            view.enlargedImage = app.sampleImage;
            view.enlargedCaption = "palette.png";
        }
        std::string editingExpected;
        if (scenario == "document-edit") {
            app.page = 1;
            app.document.setMode(cyui::DocumentMode::live);
            const auto initial = app.document.markdown();
            const auto caret = initial.find("cœur.") + std::strlen("cœur.");
            app.document.select(caret, caret);
            app.document.focus();
            editingExpected =
                initial.substr(0, caret) + "<br>Une ligne avec Maj+Entrée." + initial.substr(caret);
        }
        std::string videoExpected;
        ImGuiWindow *videoHeading = nullptr;
        float videoHeight = 0;
        if (scenario == "document-video") {
            app.page = 1;
            app.document.setMode(cyui::DocumentMode::live);
            auto initial = app.document.markdown();
            const auto caret = initial.find("Liste de vérification") + std::strlen("Liste de vérification");
            initial.insert(caret, "\n\ntestds");
            app.document.setMarkdown(initial);
            app.document.select(caret, caret);
            app.document.focus();
            videoExpected =
                initial.substr(0, caret) + "<br>gfdslkhj<br>jkjhg<br>hjgf<br>kjgh" + initial.substr(caret);
        }
        if (scenario == "million-chat") {
            app.page = 0;
            app.virtualHistory = true;
            app.conversation.jumpToBottom = true;
        }
        if (test && !selfTest(app))
            exitCode = 1;
        int frames = 0;
        while (!app.quit && !(automated && exitCode)) {
            host.events();
            try {
                app.dialogs.poll();
            } catch (const std::exception &e) {
                cyui::notify(e.what(), cyui::Tone::error);
            }
            if (!automated && (SDL_GetWindowFlags(host.window) & SDL_WINDOW_MINIMIZED)) {
                SDL_Delay(20);
                continue;
            }
            host.newFrame();
            if (scenario == "document-edit") {
                if (frames == 4 || frames == 5) {
                    io.AddKeyEvent(ImGuiMod_Shift, frames == 4);
                    io.AddKeyEvent(ImGuiKey_Enter, frames == 4);
                }
                const char *chunks[]{"Une ", "ligne ", "avec ", "Maj+Entrée."};
                if (frames >= 8 && frames <= 14 && frames % 2 == 0)
                    io.AddInputCharactersUTF8(chunks[(frames - 8) / 2]);
            }
            if (scenario == "document-video") {
                const int breaks[]{4, 18, 26, 34};
                for (const auto at : breaks)
                    if (frames == at || frames == at + 1) {
                        io.AddKeyEvent(ImGuiMod_Shift, frames == at);
                        io.AddKeyEvent(ImGuiKey_Enter, frames == at);
                    }
                const int typing[]{14, 22, 30, 38};
                const char *words[]{"gfdslkhj", "jkjhg", "hjgf", "kjgh"};
                for (int n = 0; n < 4; ++n)
                    if (frames == typing[n])
                        io.AddInputCharactersUTF8(words[n]);
                if (frames == 42) {
                    app.document.select(0, 0);
                    app.document.focus();
                }
            }
            if (scenario == "hover" && frames > 2) {
                const auto &view = app.conversation.views[app.messages.back().id];
                if (!view.hits.empty())
                    io.AddMousePosEvent(view.hits.front().bounds.min.x + cyui::dp(6),
                                        view.hits.front().bounds.min.y + cyui::dp(2));
            }
            ImGui::NewFrame();
            if (test) {
                // Exercise the actual chat, editor and reader render paths.
                app.page = frames < 12 ? 0 : 1;
                app.document.setMode(cyui::DocumentMode(std::min(3, std::max(0, (frames - 12) / 8))));
            }
            app.frame();
            if (scenario == "document-video") {
                if (frames == 3)
                    for (auto *child : GImGui->Windows)
                        if (child->ChildId == GImGui->ActiveId)
                            videoHeading = child;
                if (frames >= 4 && videoHeading) {
                    int glyphs = 0;
                    for (const auto &vertex : videoHeading->DrawList->VtxBuffer)
                        if (vertex.col == cyui::palette().text &&
                            videoHeading->InnerClipRect.Contains(vertex.pos))
                            ++glyphs;
                    if (videoHeading->Scroll.y != 0 || glyphs < 8)
                        exitCode = 1;
                }
                if (frames == 40 && videoHeading)
                    videoHeight = videoHeading->Size.y;
                if (frames >= 45 && (!videoHeading || videoHeading->Size.y != videoHeight ||
                                     app.document.markdown() != videoExpected))
                    exitCode = 1;
                if (exitCode && !capture.empty()) {
                    std::ofstream report(platform::path(capture).replace_extension(".failure.txt"));
                    report << "Video replay failed at frame " << frames << "\nExpected:\n"
                           << videoExpected << "\nActual:\n"
                           << app.document.markdown();
                }
            }
            if (scenario == "million-chat" && frames == 30 &&
                (app.conversation.lastVisible != 1'000'000 || app.conversation.fetched > 64 ||
                 app.conversation.documents.size() > 128))
                exitCode = 1;
            if (scenario == "document-edit" && frames == 30 &&
                (app.document.markdown() != editingExpected ||
                 app.document.selection().first != app.document.selection().second)) {
                std::fprintf(
                    stderr,
                    "FAIL: native document typing/Shift+Enter changed surrounding text or selected it\n");
                if (!capture.empty()) {
                    std::ofstream report(platform::path(capture).replace_extension(".failure.txt"));
                    report << "Selection: " << app.document.selection().first << ", "
                           << app.document.selection().second << "\nExpected:\n"
                           << editingExpected << "\nActual:\n"
                           << app.document.markdown();
                }
                exitCode = 1;
            }
            ImGui::Render();
            if (!host.render()) {
                exitCode = 1;
                break;
            }
            if (automated && ++frames >= captureFrames) {
                if (!capture.empty() && !host.capture(platform::path(capture)))
                    exitCode = 1;
                break;
            }
            if (!SDL_RenderPresent(host.renderer)) {
                exitCode = 1;
                break;
            }
        }
    } // Editors and asset textures die before Host tears down ImGui and SDL.
    return exitCode;
}
