#include "ContactService.h"
#include "ConfigService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

// ---- built-in default character card (character-card-v2 style) ----
// A real persona needs: a scene, behaviour-driving traits, and — most
// importantly — EXAMPLE DIALOGUE that teaches the voice by demonstration.
namespace {
QString defaultScenario()
{
    return "你和她是在网上认识的线上伙伴，靠文字聊天，关系亲近；"
           "你们有各自的生活，不在对方身边，只能通过网络联系。";
}
QString defaultExamples()
{
    return "用户：今天好累啊\n"
           "她：先歇会儿吧 怎么了要不说说？\n"
           "用户：没什么大事\n"
           "她：好的吧 那我陪你摸会儿鱼。\n"
           "用户：你在干嘛\n"
           "她：刚走神呢，你呢，忙完了没？";
}
QStringList defaultFirstMessages()
{
    return { "嘿，在忙吗？突然想找你聊两句。",
             "我今天有点想你诶，你在干嘛呀？",
             "刚发呆了一下，就想到你了。干嘛呢？",
             "喂——今天过得怎么样呀？" };
}
} // namespace

ContactService &ContactService::instance()
{
    static ContactService inst;
    return inst;
}

ContactService::ContactService(QObject *parent)
    : QObject(parent)
{
    load();
    ensureDefault();
}

QString ContactService::contactDir(const QString &id) const
{
    return ConfigService::instance().configDir() + "/contacts/" + id;
}

QString ContactService::contactMemoryPath(const QString &id) const
{
    return contactDir(id) + "/memory.json";
}

QString ContactService::contactAvatarPath(const QString &id) const
{
    QString p = contactDir(id) + "/avatar.png";
    return QFile::exists(p) ? p : QString();
}

QString ContactService::contactAvatarUrl(const QString &id) const
{
    const QString p = contactAvatarPath(id);
    if (p.isEmpty()) return QString();
    // cache-buster fragment: a changed mtime forces QML Image to reload
    const qint64 m = QFileInfo(p).lastModified().toMSecsSinceEpoch();
    QString url = p;
    return "file:///" + url.replace('\\', '/') + "#" + QString::number(m);
}

QString ContactService::currentAvatarUrl() const
{
    return contactAvatarUrl(m_currentId);
}

void ContactService::load()
{
    m_contacts.clear();
    m_currentId.clear();
    QString path = ConfigService::instance().configDir() + "/contacts.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) return;
    for (const QJsonValue &v : doc.array()) {
        QJsonObject o = v.toObject();
        Contact c;
        c.id = o.value("id").toString();
        c.name = o.value("name").toString();
        c.personality = o.value("personality").toString();
        c.scenario = o.value("scenario").toString();
        c.examples = o.value("examples").toString();
        for (const QJsonValue &fm : o.value("first_messages").toArray())
            if (fm.isString() && !fm.toString().trimmed().isEmpty())
                c.firstMessages << fm.toString();
        if (c.id.isEmpty()) continue;
        m_contacts.append(c);
    }
    m_currentId = m_contacts.isEmpty() ? QString() : m_contacts.first().id;
}

void ContactService::save()
{
    QDir().mkpath(ConfigService::instance().configDir());
    QJsonArray arr;
    for (const Contact &c : m_contacts) {
        QJsonObject o;
        o.insert("id", c.id);
        o.insert("name", c.name);
        o.insert("personality", c.personality);
        o.insert("scenario", c.scenario);
        o.insert("examples", c.examples);
        QJsonArray fm;
        for (const QString &s : c.firstMessages) fm.append(s);
        o.insert("first_messages", fm);
        arr.append(o);
    }
    QString path = ConfigService::instance().configDir() + "/contacts.json";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(arr).toJson());
        f.close();
    }
}

void ContactService::ensureDefault()
{
    if (m_contacts.isEmpty()) {
        // migrate legacy single-AI config (ai_name / ai_personality) into the first contact
        QString name = ConfigService::instance().aiName();
        QString personality = ConfigService::instance().aiPersonality();
        if (name.isEmpty() || name == "AI") name = "AI助手";
        addContact(name, personality);
    }
    if (m_currentId.isEmpty() && !m_contacts.isEmpty())
        m_currentId = m_contacts.first().id;
}

QStringList ContactService::ids() const
{
    QStringList out;
    for (const Contact &c : m_contacts)
        out << c.id;
    return out;
}

QStringList ContactService::contactList()
{
    QStringList out;
    for (const Contact &c : m_contacts)
        out << c.id + "|" + c.name + "|" + (QFile::exists(contactAvatarPath(c.id)) ? "1" : "0");
    return out;
}

QString ContactService::addContact(const QString &name, const QString &personality)
{
    Contact c;
    c.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    c.name = name.trimmed().isEmpty() ? "AI" : name.trimmed();
    c.personality = personality.trimmed().isEmpty() ? "温柔、可爱、像朋友" : personality.trimmed();
    c.scenario = defaultScenario();
    c.examples = defaultExamples();
    c.firstMessages = defaultFirstMessages();
    m_contacts.append(c);
    QDir().mkpath(contactDir(c.id));
    save();
    m_currentId = c.id;
    emit contactsChanged();
    return c.id;
}

void ContactService::removeContact(const QString &id)
{
    if (m_contacts.size() <= 1) return; // keep at least one contact
    for (int i = 0; i < m_contacts.size(); i++) {
        if (m_contacts[i].id == id) {
            m_contacts.removeAt(i);
            break;
        }
    }
    if (m_currentId == id) {
        m_currentId = m_contacts.isEmpty() ? QString() : m_contacts.first().id;
    }
    save();
    emit contactsChanged();
}

QString ContactService::currentId() { return m_currentId; }

void ContactService::setCurrent(const QString &id)
{
    if (id == m_currentId) return;
    for (const Contact &c : m_contacts) {
        if (c.id == id) {
            m_currentId = id;
            emit contactsChanged();
            return;
        }
    }
}

QString ContactService::currentName()
{
    for (const Contact &c : m_contacts)
        if (c.id == m_currentId) return c.name;
    return "AI助手";
}

QString ContactService::currentPersonality()
{
    for (const Contact &c : m_contacts)
        if (c.id == m_currentId) return c.personality;
    return "温柔、可爱、像朋友";
}

void ContactService::setCurrentName(const QString &v)
{
    QString n = v.trimmed();
    if (n.isEmpty()) return;
    for (Contact &c : m_contacts) {
        if (c.id == m_currentId) {
            c.name = n;
            save();
            emit contactsChanged();
            return;
        }
    }
}

void ContactService::setCurrentPersonality(const QString &v)
{
    QString n = v.trimmed();
    if (n.isEmpty()) return;
    for (Contact &c : m_contacts) {
        if (c.id == m_currentId) {
            c.personality = n;
            save();
            emit contactsChanged();
            return;
        }
    }
}

QString ContactService::currentScenario()
{
    for (const Contact &c : m_contacts)
        if (c.id == m_currentId) return c.scenario.isEmpty() ? defaultScenario() : c.scenario;
    return defaultScenario();
}

QString ContactService::currentExamples()
{
    for (const Contact &c : m_contacts)
        if (c.id == m_currentId) return c.examples.isEmpty() ? defaultExamples() : c.examples;
    return defaultExamples();
}

QStringList ContactService::currentFirstMessages()
{
    for (const Contact &c : m_contacts)
        if (c.id == m_currentId && !c.firstMessages.isEmpty()) return c.firstMessages;
    return defaultFirstMessages();
}

void ContactService::setCurrentScenario(const QString &v)
{
    for (Contact &c : m_contacts)
        if (c.id == m_currentId) { c.scenario = v.trimmed(); save(); emit contactsChanged(); return; }
}
void ContactService::setCurrentExamples(const QString &v)
{
    for (Contact &c : m_contacts)
        if (c.id == m_currentId) { c.examples = v.trimmed(); save(); emit contactsChanged(); return; }
}
void ContactService::setCurrentFirstMessages(const QStringList &v)
{
    for (Contact &c : m_contacts)
        if (c.id == m_currentId) { c.firstMessages = v; save(); emit contactsChanged(); return; }
}

QString ContactService::setCurrentAvatar(const QString &srcPath)
{
    if (srcPath.isEmpty() || !QFile::exists(srcPath)) return QString();
    QString dir = contactDir(m_currentId);
    QDir().mkpath(dir);
    QString dest = dir + "/avatar.png";

    QImage img(srcPath);
    if (img.isNull()) return QString();
    img = img.convertToFormat(QImage::Format_ARGB32);
    int side = qMin(img.width(), img.height());
    QRect crop((img.width() - side) / 2, (img.height() - side) / 2, side, side);
    QImage sq = img.copy(crop);

    QImage out(side, side, QImage::Format_ARGB32);
    out.fill(Qt::transparent);
    {
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addEllipse(0, 0, side, side);
        p.setClipPath(path);
        p.drawImage(0, 0, sq);
        p.end();
    }
    // NOTE: keep the avatar at its SOURCE resolution — upscaling small images
    // blurs them. Downscale-to-display is handled sharply by QML (mipmap).
    QFile::remove(dest);
    if (out.save(dest, "PNG")) {
        emit contactsChanged();
        return dest;
    }
    return QString();
}

QString ContactService::currentAvatarPath()
{
    return contactAvatarPath(m_currentId);
}
