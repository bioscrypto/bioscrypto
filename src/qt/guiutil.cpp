#include <QApplication>

#include "guiutil.h"

#include "bitcoinaddressvalidator.h"
#include "walletmodel.h"
#include "bitcoinunits.h"

#include "util.h"
#include "init.h"

#include <QDateTime>
#include <QDoubleValidator>
#include <QFont>
#include <QLineEdit>
#include <QUrl>
#include <QTextDocument> // For Qt::escape
#include <QAbstractItemView>
#include <QClipboard>
#include <QFileDialog>
#include <QDesktopServices>
#include <QThread>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "shlwapi.h"
#include "shlobj.h"
#include "shellapi.h"
#endif

namespace GUIUtil {

QString dateTimeStr(const QDateTime &date)
{
    return date.date().toString(Qt::SystemLocaleShortDate) + QString(" ") + date.toString("hh:mm");
}

QString dateTimeStr(qint64 nTime)
{
    return dateTimeStr(QDateTime::fromTime_t((qint32)nTime));
}

QFont bitcoinAddressFont()
{
    QFont font("Monospace");
#if QT_VERSION >= 0x040800
    font.setStyleHint(QFont::Monospace);
#else
    font.setStyleHint(QFont::TypeWriter);
#endif
    return font;
}

void setupAddressWidget(QLineEdit *widget, QWidget *parent)
{
    widget->setMaxLength(BitcoinAddressValidator::MaxAddressLength);
    widget->setValidator(new BitcoinAddressValidator(parent));
    widget->setFont(bitcoinAddressFont());
}

void setupAmountWidget(QLineEdit *widget, QWidget *parent)
{
    QDoubleValidator *amountValidator = new QDoubleValidator(parent);
    amountValidator->setDecimals(8);
    amountValidator->setBottom(0.0);
    widget->setValidator(amountValidator);
    widget->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
}

bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out)
{
    // NovaCoin: check prefix
    if(uri.scheme() != QString("bioscrypto"))
        return false;

    SendCoinsRecipient rv;
    rv.address = uri.path();
    rv.amount = 0;
    QList<QPair<QString, QString> > items = uri.queryItems();
    for (QList<QPair<QString, QString> >::iterator i = items.begin(); i != items.end(); i++)
    {
        bool fShouldReturnFalse = false;
        if (i->first.startsWith("req-"))
        {
            i->first.remove(0, 4);
            fShouldReturnFalse = true;
        }

        if (i->first == "label")
        {
            rv.label = i->second;
            fShouldReturnFalse = false;
        }
        else if (i->first == "amount")
        {
            if(!i->second.isEmpty())
            {
                if(!BitcoinUnits::parse(BitcoinUnits::BTC, i->second, &rv.amount))
                {
                    return false;
                }
            }
            fShouldReturnFalse = false;
        }

        if (fShouldReturnFalse)
            return false;
    }
    if(out)
    {
        *out = rv;
    }
    return true;
}

bool parseBitcoinURI(QString uri, SendCoinsRecipient *out)
{
    // Convert bioscrypto:// to bioscrypto:
    //
    //    Cannot handle this later, because bitcoin:// will cause Qt to see the part after // as host,
    //    which will lower-case it (and thus invalidate the address).
    if(uri.startsWith("bioscrypto://"))
    {
        uri.replace(0, 12, "bioscrypto:");
    }
    QUrl uriInstance(uri);
    return parseBitcoinURI(uriInstance, out);
}

QString HtmlEscape(const QString& str, bool fMultiLine)
{
    QString escaped = Qt::escape(str);
    if(fMultiLine)
    {
        escaped = escaped.replace("\n", "<br>\n");
    }
    return escaped;
}

QString HtmlEscape(const std::string& str, bool fMultiLine)
{
    return HtmlEscape(QString::fromStdString(str), fMultiLine);
}

void copyEntryData(QAbstractItemView *view, int column, int role)
{
    if(!view || !view->selectionModel())
        return;
    QModelIndexList selection = view->selectionModel()->selectedRows(column);

    if(!selection.isEmpty())
    {
        // Copy first item
        QApplication::clipboard()->setText(selection.at(0).data(role).toString());
    }
}

QString getSaveFileName(QWidget *parent, const QString &caption,
                                 const QString &dir,
                                 const QString &filter,
                                 QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
        myDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    }
    else
    {
        myDir = dir;
    }
    QString result = QFileDialog::getSaveFileName(parent, caption, myDir, filter, &selectedFilter);

    /* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar ...) */
    QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
    QString selectedSuffix;
    if(filter_re.exactMatch(selectedFilter))
    {
        selectedSuffix = filter_re.cap(1);
    }

    /* Add suffix if needed */
    QFileInfo info(result);
    if(!result.isEmpty())
    {
        if(info.suffix().isEmpty() && !selectedSuffix.isEmpty())
        {
            /* No suffix specified, add selected suffix */
            if(!result.endsWith("."))
                result.append(".");
            result.append(selectedSuffix);
        }
    }

    /* Return selected suffix if asked to */
    if(selectedSuffixOut)
    {
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

Qt::ConnectionType blockingGUIThreadConnection()
{
    if(QThread::currentThread() != QCoreApplication::instance()->thread())
    {
        return Qt::BlockingQueuedConnection;
    }
    else
    {
        return Qt::DirectConnection;
    }
}

bool checkPoint(const QPoint &p, const QWidget *w)
{
    QWidget *atW = qApp->widgetAt(w->mapToGlobal(p));
    if (!atW) return false;
    return atW->topLevelWidget() == w;
}

bool isObscured(QWidget *w)
{
    return !(checkPoint(QPoint(0, 0), w)
        && checkPoint(QPoint(w->width() - 1, 0), w)
        && checkPoint(QPoint(0, w->height() - 1), w)
        && checkPoint(QPoint(w->width() - 1, w->height() - 1), w)
        && checkPoint(QPoint(w->width() / 2, w->height() / 2), w));
}

void openDebugLogfile()
{
    boost::filesystem::path pathDebug = GetDataDir() / "debug.log";

    /* Open debug.log with the associated application */
    if (boost::filesystem::exists(pathDebug))
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(pathDebug.string())));
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int size_threshold, QObject *parent) :
    QObject(parent), size_threshold(size_threshold)
{

}

bool ToolTipToRichTextFilter::eventFilter(QObject *obj, QEvent *evt)
{
    if(evt->type() == QEvent::ToolTipChange)
    {
        QWidget *widget = static_cast<QWidget*>(obj);
        QString tooltip = widget->toolTip();
        if(tooltip.size() > size_threshold && !tooltip.startsWith("<qt>") && !Qt::mightBeRichText(tooltip))
        {
            // Prefix <qt/> to make sure Qt detects this as rich text
            // Escape the current message as HTML and replace \n by <br>
            tooltip = "<qt>" + HtmlEscape(tooltip, true) + "<qt/>";
            widget->setToolTip(tooltip);
            return true;
        }
    }
    return QObject::eventFilter(obj, evt);
}

#ifdef WIN32
boost::filesystem::path static StartupShortcutPath()
{
    return GetSpecialFolderPath(CSIDL_STARTUP) / "BiosCrypto.lnk";
}

bool GetStartOnSystemStartup()
{
    // check for Bitcoin.lnk
    return boost::filesystem::exists(StartupShortcutPath());
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    // If the shortcut exists already, remove it for updating
    boost::filesystem::remove(StartupShortcutPath());

    if (fAutoStart)
    {
        CoInitialize(NULL);

        // Get a pointer to the IShellLink interface.
        IShellLink* psl = NULL;
        HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL,
                                CLSCTX_INPROC_SERVER, IID_IShellLink,
                                reinterpret_cast<void**>(&psl));

        if (SUCCEEDED(hres))
        {
            // Get the current executable path
            TCHAR pszExePath[MAX_PATH];
            GetModuleFileName(NULL, pszExePath, sizeof(pszExePath));

            TCHAR pszArgs[5] = TEXT("-min");

            // Set the path to the shortcut target
            psl->SetPath(pszExePath);
            PathRemoveFileSpec(pszExePath);
            psl->SetWorkingDirectory(pszExePath);
            psl->SetShowCmd(SW_SHOWMINNOACTIVE);
            psl->SetArguments(pszArgs);

            // Query IShellLink for the IPersistFile interface for
            // saving the shortcut in persistent storage.
            IPersistFile* ppf = NULL;
            hres = psl->QueryInterface(IID_IPersistFile,
                                       reinterpret_cast<void**>(&ppf));
            if (SUCCEEDED(hres))
            {
                WCHAR pwsz[MAX_PATH];
                // Ensure that the string is ANSI.
                MultiByteToWideChar(CP_ACP, 0, StartupShortcutPath().string().c_str(), -1, pwsz, MAX_PATH);
                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(pwsz, TRUE);
                ppf->Release();
                psl->Release();
                CoUninitialize();
                return true;
            }
            psl->Release();
        }
        CoUninitialize();
        return false;
    }
    return true;
}

#elif defined(Q_OS_LINUX)

// Follow the Desktop Application Autostart Spec:
//  http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html

boost::filesystem::path static GetAutostartDir()
{
    namespace fs = boost::filesystem;

    char* pszConfigHome = getenv("XDG_CONFIG_HOME");
    if (pszConfigHome) return fs::path(pszConfigHome) / "autostart";
    char* pszHome = getenv("HOME");
    if (pszHome) return fs::path(pszHome) / ".config" / "autostart";
    return fs::path();
}

boost::filesystem::path static GetAutostartFilePath()
{
    return GetAutostartDir() / "bioscrypto.desktop";
}

bool GetStartOnSystemStartup()
{
    boost::filesystem::ifstream optionFile(GetAutostartFilePath());
    if (!optionFile.good())
        return false;
    // Scan through file for "Hidden=true":
    std::string line;
    while (!optionFile.eof())
    {
        getline(optionFile, line);
        if (line.find("Hidden") != std::string::npos &&
            line.find("true") != std::string::npos)
            return false;
    }
    optionFile.close();

    return true;
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    if (!fAutoStart)
        boost::filesystem::remove(GetAutostartFilePath());
    else
    {
        char pszExePath[MAX_PATH+1];
        memset(pszExePath, 0, sizeof(pszExePath));
        if (readlink("/proc/self/exe", pszExePath, sizeof(pszExePath)-1) == -1)
            return false;

        boost::filesystem::create_directories(GetAutostartDir());

        boost::filesystem::ofstream optionFile(GetAutostartFilePath(), std::ios_base::out|std::ios_base::trunc);
        if (!optionFile.good())
            return false;
        // Write a bitcoin.desktop file to the autostart directory:
        optionFile << "[Desktop Entry]\n";
        optionFile << "Type=Application\n";
        optionFile << "Name=BiosCrypto\n";
        optionFile << "Exec=" << pszExePath << " -min\n";
        optionFile << "Terminal=false\n";
        optionFile << "Hidden=false\n";
        optionFile.close();
    }
    return true;
}
#else

// TODO: OSX startup stuff; see:
// https://developer.apple.com/library/mac/#documentation/MacOSX/Conceptual/BPSystemStartup/Articles/CustomLogin.html

bool GetStartOnSystemStartup() { return false; }
bool SetStartOnSystemStartup(bool fAutoStart) { return false; }

#endif

HelpMessageBox::HelpMessageBox(QWidget *parent) :
    QMessageBox(parent)
{
    header = tr("BiosCrypto-Qt") + " " + tr("version") + " " +
        QString::fromStdString(FormatFullVersion()) + "\n\n" +
        tr("Usage:") + "\n" +
        "  bioscrypto-qt [" + tr("command-line options") + "]                     " + "\n";

    coreOptions = QString::fromStdString(HelpMessage());

    uiOptions = tr("UI options") + ":\n" +
        "  -lang=<lang>           " + tr("Set language, for example \"de_DE\" (default: system locale)") + "\n" +
        "  -min                   " + tr("Start minimized") + "\n" +
        "  -splash                " + tr("Show splash screen on startup (default: 1)") + "\n";

    setWindowTitle(tr("BiosCrypto-Qt"));
    setTextFormat(Qt::PlainText);
    // setMinimumWidth is ignored for QMessageBox so put in non-breaking spaces to make it wider.
    setText(header + QString(QChar(0x2003)).repeated(50));
    setDetailedText(coreOptions + "\n" + uiOptions);
}

void HelpMessageBox::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    QString strUsage = header + "\n" + coreOptions + "\n" + uiOptions;
    fprintf(stdout, "%s", strUsage.toStdString().c_str());
}

void HelpMessageBox::showOrPrint()
{
#if defined(WIN32)
        // On Windows, show a message box, as there is no stderr/stdout in windowed applications
        exec();
#else
        // On other operating systems, print help text to console
        printToConsole();
#endif
}

void SetThemeQSS(QApplication& app)
{
    app.setStyleSheet("QWidget        { background: rgb(118, 134, 131); }"

                      "QFrame         { border: none; }"

                      "QLineEdit      { background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); selection-background-color: rgba(0, 0, 0, 80); height: 24px; border: none; border-radius: 3px; }"

                      "QTextEdit      { background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); selection-background-color: rgba(0, 0, 0, 80); height: 24px; border: none; border-radius: 3px; }"

                      "QPlainTextEdit { background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); selection-background-color: rgba(0, 0, 0, 80); height: 24px; border: none; border-radius: 3px; }"

                      "QLabel         { color: rgba(252, 252, 252, 200); }"

                      "QPushButton                  { background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); border: none; padding-left: 12px; padding-right: 12px; padding-top: 8px; padding-bottom: 8px; height: 16px; border-radius: 3px; min-width: 64px; }"
                      "QPushButton:enabled:hover    { background: rgba(252, 252, 252, 120); color: rgb(118, 134, 131); }"
                      "QPushButton:enabled:pressed  { background: rgba(252, 252, 252, 40); color: rgb(220, 220, 220); }"
                      "QPushButton:disabled         { background: rgb(0, 0, 0, 20); color: rgb(150, 150, 150); }"

                      "QCheckBox                          { color: rgba(252, 252, 252, 200); }"
                      "QCheckBox::indicator               { width: 12px; height: 12px; }"
                      "QCheckBox::indicator::checked      { image: url(:/icons/cb_checked); }"
                      "QCheckBox::indicator::unchecked    { image: url(:/icons/cb_unchecked); }"

                      "QRadioButton                       { color: rgba(252, 252, 252, 200); }"
                      "QRadioButton::indicator            { width: 12px; height: 12px; }"
                      "QRadioButton::indicator::checked   { image: url(:/icons/rb_checked); }"
                      "QRadioButton::indicator::unchecked { image: url(:/icons/rb_unchecked); }"

                      "QDoubleSpinBox               { background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); selection-background-color: rgba(0, 0, 0, 80); height: 24px; border: none; border-radius: 3px; }"
                      "QDoubleSpinBox::up-arrow     { width: 8px; height: 8px; image: url(:/icons/up_arrow); }"
                      "QDoubleSpinBox::down-arrow   { width: 8px; height: 8px; image: url(:/icons/down_arrow); }"
                      "QDoubleSpinBox::up-button    { width: 12px; height: 12px; background: rgba(0, 0, 0, 40); border: none; border-top-right-radius: 3px; }"
                      "QDoubleSpinBox::down-button  { width: 12px; height: 12px; background: rgba(0, 0, 0, 40); border: none; border-bottom-right-radius: 3px; }"
                      "QDoubleSpinBox::down-button:hover, QDoubleSpinBox::up-button:hover     { background: rgba(0, 0, 0, 80); }"
                      "QDoubleSpinBox::down-button:pressed, QDoubleSpinBox::up-button:pressed { background: rgba(0, 0, 0, 120); }"

                      "QComboBox                         { background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); selection-background-color: rgba(0, 0, 0, 80); height: 24px; border: none; border-radius: 3px; }"
                      "QComboBox QAbstractItemView::item { color: rgba(252, 252, 252, 200); }"
                      "QComboBox::down-arrow             { width: 12px; height: 12px; image: url(:/icons/down_arrow); }"
                      "QComboBox::drop-down              { background: rgba(0, 0, 0, 40); border: none; border-top-right-radius: 3px; border-bottom-right-radius: 3px; }"
                      "QComboBox::drop-down:hover        { background: rgba(0, 0, 0, 80); }"
                      "QComboBox::drop-down:pressed      { background: rgba(0, 0, 0, 120); }"

                      "QValueComboBox { background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); height: 24px; border: none; border-radius: 3px; }"

                      "QDialogButtonBox { height: 16px; }"

                      "QMenuBar                { background: rgb(118, 134, 131); color: rgba(252, 252, 252, 200); }"
                      "QMenuBar::item          { background: transparent; }"
                      "QMenuBar::item:hover    { background: rgba(252, 252, 252, 120); color: rgba(0, 0, 0, 150); }"
                      "QMenuBar::item:selected { background: rgb(0, 0, 0, 60); color: rgba(252, 252, 252, 200); }"

                      "QMenu                   { background: rgb(135, 156, 152); color: rgba(252, 252, 252, 200); }"
                      "QMenu::item:hover       { background: rgba(252, 252, 252, 120); color: rgba(0, 0, 0, 150); }"
                      "QMenu::item:selected    { background: rgb(0, 0, 0, 60); color: rgba(252, 252, 252, 200); }"

                      "QScrollBar                         { color: rgba(252, 252, 252, 200); }"
                      "QScrollBar:vertical                { margin: 16px 0px 16px 0px; width: 16px; background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); border: none; border-radius: 3px; }"
                      "QScrollBar::handle:vertical        { background: rgba(0, 0, 0, 10); border: none; border-radius: 3px; min-height: 16px; }"
                      "QScrollBar::add-line:vertical      { background: rgba(0, 0, 0, 20); height: 16px; border: none; border-radius: 3px; subcontrol-position: bottom; subcontrol-origin: margin; }"
                      "QScrollBar::sub-line:vertical      { background: rgba(0, 0, 0, 20); height: 16px; border: none; border-radius: 3px; subcontrol-position: 16px; subcontrol-origin: margin; }"
                      "QScrollBar::up-arrow:vertical      { width: 12px; height: 12px; image: url(:/icons/up_arrow); }"
                      "QScrollBar::down-arrow:vertical    { width: 12px; height: 12px; image: url(:/icons/down_arrow); }"
                      "QScrollBar:horizontal              { margin: 0px 16px 0px 16px; height: 16px; background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); border: none; border-radius: 3px; }"
                      "QScrollBar::handle:horizontal      { background: rgba(0, 0, 0, 10); border: none; border-radius: 3px; min-width: 16px; }"
                      "QScrollBar::add-line:horizontal    { background: rgba(0, 0, 0, 20); width: 16px; border: none; border-radius: 3px; subcontrol-position: right; subcontrol-origin: margin; }"
                      "QScrollBar::sub-line:horizontal    { background: rgba(0, 0, 0, 20); width: 16px; border: none; border-radius: 3px; subcontrol-position: left; subcontrol-origin: margin; }"
                      "QScrollBar::left-arrow:horizontal  { width: 12px; height: 12px; image: url(:/icons/left_arrow); }"
                      "QScrollBar::right-arrow:horizontal { width: 12px; height: 12px; image: url(:/icons/right_arrow); }"

                      "QSlider::groove:horizontal { background: rgba(0, 0, 0, 80); height: 16px; border: none; border-radius: 3px; }"
                      "QSlider::handle:horizontal { background: rgba(0, 0, 0, 0); width: 32px; height: 16px; image: url(:/icons/slider_handle); }"

                      "QTabWidget             { background: rgba(252, 252, 252, 40); border: none; }"
                      "QTabWidget::pane       { border: none; }"

                      "QTabBar::tab           { color: rgba(252, 252, 252, 200); border: none; border-top-right-radius: 3px; border-top-left-radius: 3px; padding: 10px; }"
                      "QTabBar::tab:selected  { background: rgba(252, 252, 252, 40); }"
                      "QTabBar::tab:!selected { color: rgba(252, 252, 252, 120); background: rgba(0, 0, 0, 10); margin-top: 2px; }"

                      "QToolBar            { background: rgb(118, 134, 131); border: none; }"
                      "QToolButton         { font-size: 11pt; background: rgba(0, 0, 0, 0); color: rgba(252, 252, 252, 120); height: 32px; border: none; border-left-color: rgba(252, 252, 252, 0); border-left-style: solid; border-left-width: 8px; margin-top: 1px; margin-bottom: 1px; }"
                      "QToolButton:hover   { background: rgba(252, 252, 252, 20); color: rgba(252, 252, 252, 120); border: none; border-left-color: rgba(252, 252, 252, 0); border-left-style: solid; border-left-width: 8px; }"
                      "QToolButton:checked { background: rgba(252, 252, 252, 40); color: rgba(252, 252, 252, 200); border: none; border-left-color: rgba(252, 252, 252, 120); border-left-style: solid; border-left-width: 8px; }"

                      "QProgressBar        { background: rgba(0, 0, 0, 40); color: rgba(252, 252, 252, 200); height: 24px; border: none; border-radius: 3px; }"
                      "QProgressBar::chunk { background: rgba(252, 252, 252, 120); color: rgba(0, 0, 0, 40); }"

                      "QHeaderView          { background: rgb(118, 134, 131); color: rgba(252, 252, 252, 200); border: none; gridline-color: rgb(135, 156, 152); }"
                      "QHeaderView::section { background: rgba(0, 0, 0, 30); color: rgba(252, 252, 252, 200); border: none; }"

                      "QTreeView                { background: rgba(0, 0, 0, 0); color: rgba(252, 252, 252, 200); gridline-color: rgb(118, 134, 131); }"
                      "QTreeView::item          { background: rgba(0, 0, 0, 0); color: rgba(252, 252, 252, 200); }"
                      "QTreeView::item:hover    { background: rgba(252, 252, 252, 120); color: rgba(0, 0, 0, 150); }"
                      "QTreeView::item:selected { background: rgba(0, 0, 0, 60); color: rgba(252, 252, 252, 200); }"
                      "QTreeView::branch:hover  { background: rgba(252, 252, 252, 120); color: rgba(0, 0, 0, 150); }"
                      "QTreeView::item:selected { background: rgba(0, 0, 0, 60); color: rgba(252, 252, 252, 200); }"

                      "QTableView                { background: rgba(0, 0, 0, 0); color: rgba(252, 252, 252, 200); gridline-color: rgb(118, 134, 131); }"
                      "QTableView::item          { background: rgba(0, 0, 0, 10); color: rgba(252, 252, 252, 200); }"
                      "QTableView::item:hover    { background: rgba(252, 252, 252, 120); color: rgba(0, 0, 0, 150); }"
                      "QTableView::item:selected { background: rgba(0, 0, 0, 60); color: rgba(252, 252, 252, 200); }"

                      "QListView                { background: rgb(135, 156, 152); color: rgba(252, 252, 252, 200); }"
                      "QListView::item          { background: transparent; }"
                      "QListView::item:hover    { background: rgba(252, 252, 252, 120); color: rgba(0, 0, 0, 150); }"
                      "QListView::item:selected { background: rgba(0, 0, 0, 60); color: rgba(252, 252, 252, 200); }"
    );
}

} // namespace GUIUtil

