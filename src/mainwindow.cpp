#include "mainwindow.h"

#include "chatbubble.h"
#include "ollamaclient.h"
#include "piperspeaker.h"
#include "speechinput.h"

#include <QAudioDevice>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_client = new OllamaClient(this);
    m_speech = new SpeechInput(this);
    m_piper = new PiperSpeaker(this);

    buildUi();
    setWindowTitle(QStringLiteral("Ollama Chat"));
    resize(960, 720);
    setMinimumSize(640, 480);

    connect(m_client, &OllamaClient::responseChunk, this, &MainWindow::onResponseChunk);
    connect(m_client, &OllamaClient::responseFinished, this, &MainWindow::onResponseFinished);
    connect(m_client, &OllamaClient::errorOccurred, this, &MainWindow::onClientError);
    connect(m_speech, &SpeechInput::recordingChanged, this, &MainWindow::onRecordingChanged);
    connect(m_speech, &SpeechInput::partialTranscription, this, &MainWindow::onPartialTranscription);
    connect(m_speech, &SpeechInput::transcriptionReady, this, &MainWindow::onTranscriptionReady);
    connect(m_speech, &SpeechInput::errorOccurred, this, &MainWindow::onSpeechError);
    connect(m_speech, &SpeechInput::statusChanged, this, &MainWindow::onSpeechStatus);
    connect(m_piper, &PiperSpeaker::speakingChanged, this, &MainWindow::onPiperSpeakingChanged);
    connect(m_piper, &PiperSpeaker::statusChanged, this, &MainWindow::onPiperStatus);
    connect(m_piper, &PiperSpeaker::errorOccurred, this, &MainWindow::onPiperError);

    refreshMicrophones();
    refreshSpeakers();

    addMessageBubble(ChatRole::Assistant,
                     QStringLiteral("Hi! Choose a model above and ask anything. "
                                    "Make sure Ollama is running on this machine."));
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralRoot"));
    setCentralWidget(central);

    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *header = new QFrame(central);
    header->setObjectName(QStringLiteral("headerBar"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 14, 20, 14);
    headerLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Ollama Chat"), header);
    title->setObjectName(QStringLiteral("appTitle"));

    m_modelCombo = new QComboBox(header);
    m_modelCombo->setObjectName(QStringLiteral("modelCombo"));
    m_modelCombo->addItem(QStringLiteral("ornith:9b"));
    m_modelCombo->addItem(QStringLiteral("qwen3.5:4b"));
    m_modelCombo->setCurrentText(QStringLiteral("ornith:9b"));
    m_modelCombo->setToolTip(QStringLiteral("Model used for the next reply"));

    auto *wordLimitLabel = new QLabel(QStringLiteral("Word limit:"), header);
    wordLimitLabel->setObjectName(QStringLiteral("wordLimitLabel"));

    m_wordLimitCombo = new QComboBox(header);
    m_wordLimitCombo->setObjectName(QStringLiteral("wordLimitCombo"));
    m_wordLimitCombo->addItems({QStringLiteral("50"), QStringLiteral("75"), QStringLiteral("100"),
                                QStringLiteral("125"), QStringLiteral("150"), QStringLiteral("200")});
    m_wordLimitCombo->setCurrentText(QStringLiteral("75"));
    m_wordLimitCombo->setToolTip(QStringLiteral("Maximum words the assistant should use in each reply"));

    m_listenButton = new QPushButton(QStringLiteral("Listen"), header);
    m_listenButton->setObjectName(QStringLiteral("listenButton"));
    m_listenButton->setCheckable(true);
    m_listenButton->setChecked(true);
    m_listenButton->setToolTip(QStringLiteral("Speak each assistant reply with Piper"));

    auto *newChatButton = new QPushButton(QStringLiteral("New chat"), header);
    newChatButton->setObjectName(QStringLiteral("newChatButton"));
    connect(newChatButton, &QPushButton::clicked, this, &MainWindow::onNewChatClicked);

    m_statusLabel = new QLabel(QStringLiteral("Ready"), header);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));

    headerLayout->addWidget(title);
    headerLayout->addSpacing(8);
    headerLayout->addWidget(m_modelCombo);
    headerLayout->addWidget(wordLimitLabel);
    headerLayout->addWidget(m_wordLimitCombo);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_statusLabel);
    headerLayout->addWidget(m_listenButton);
    headerLayout->addWidget(newChatButton);

    m_chatScroll = new QScrollArea(central);
    m_chatScroll->setObjectName(QStringLiteral("chatScroll"));
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_messageContainer = new QWidget(m_chatScroll);
    m_messageContainer->setObjectName(QStringLiteral("messageContainer"));
    m_messageLayout = new QVBoxLayout(m_messageContainer);
    m_messageLayout->setContentsMargins(0, 16, 0, 16);
    m_messageLayout->setSpacing(4);
    m_messageLayout->addStretch(1);
    m_chatScroll->setWidget(m_messageContainer);

    auto *inputBar = new QFrame(central);
    inputBar->setObjectName(QStringLiteral("inputBar"));
    auto *inputOuter = new QVBoxLayout(inputBar);
    inputOuter->setContentsMargins(20, 12, 20, 20);
    inputOuter->setSpacing(10);

    auto *inputRow = new QHBoxLayout();
    inputRow->setSpacing(10);

    m_micCombo = new QComboBox(inputBar);
    m_micCombo->setObjectName(QStringLiteral("micCombo"));
    m_micCombo->setToolTip(QStringLiteral("Microphone used for speech to text"));
    connect(m_micCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onMicSelectionChanged);

    m_refreshMicsButton = new QPushButton(QStringLiteral("↻"), inputBar);
    m_refreshMicsButton->setObjectName(QStringLiteral("refreshMicsButton"));
    m_speakerCombo = new QComboBox(inputBar);
    m_speakerCombo->setObjectName(QStringLiteral("speakerCombo"));
    m_speakerCombo->setToolTip(QStringLiteral("Speaker used for Piper playback"));
    connect(m_speakerCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onSpeakerSelectionChanged);

    m_refreshMicsButton->setToolTip(QStringLiteral("Refresh microphones and speakers"));
    connect(m_refreshMicsButton, &QPushButton::clicked, this, &MainWindow::onRefreshMicsClicked);

    m_recordButton = new QPushButton(QStringLiteral("Mic"), inputBar);
    m_recordButton->setObjectName(QStringLiteral("recordButton"));
    m_recordButton->setToolTip(QStringLiteral("Start or stop voice input"));
    connect(m_recordButton, &QPushButton::clicked, this, &MainWindow::onRecordClicked);

    m_input = new QTextEdit(inputBar);
    m_input->setObjectName(QStringLiteral("messageInput"));
    m_input->setPlaceholderText(QStringLiteral("Message Ollama…  (Enter to send, Shift+Enter for new line, Mic for voice)"));
    m_input->setFixedHeight(52);
    m_input->setTabChangesFocus(false);
    m_input->installEventFilter(this);

    m_sendButton = new QPushButton(QStringLiteral("↑"), inputBar);
    m_sendButton->setObjectName(QStringLiteral("sendButton"));
    m_sendButton->setToolTip(QStringLiteral("Send message"));
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);

    inputRow->addWidget(m_micCombo, 0, Qt::AlignBottom);
    inputRow->addWidget(m_speakerCombo, 0, Qt::AlignBottom);
    inputRow->addWidget(m_refreshMicsButton, 0, Qt::AlignBottom);
    inputRow->addWidget(m_recordButton, 0, Qt::AlignBottom);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_sendButton, 0, Qt::AlignBottom);

    m_stopButton = new QPushButton(QStringLiteral("Stop generating"), inputBar);
    m_stopButton->setObjectName(QStringLiteral("stopButton"));
    m_stopButton->setVisible(false);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);

    inputOuter->addLayout(inputRow);
    inputOuter->addWidget(m_stopButton, 0, Qt::AlignHCenter);

    rootLayout->addWidget(header);
    rootLayout->addWidget(m_chatScroll, 1);
    rootLayout->addWidget(inputBar);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
                onSendClicked();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onSendClicked()
{
    if (m_client->isBusy()) {
        return;
    }

    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }

    m_input->clear();

    addMessageBubble(ChatRole::User, text);

    QJsonObject userMessage;
    userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMessage.insert(QStringLiteral("content"), text);
    m_messages.append(userMessage);

    m_activeAssistantBubble = addMessageBubble(ChatRole::Assistant, QString());
    m_streamBuffer.clear();

    setGenerating(true);
    m_client->sendChat(m_modelCombo->currentText(), conversationForApi());
}

void MainWindow::onNewChatClicked()
{
    if (m_client->isBusy()) {
        m_client->abortRequest();
    }
    m_piper->stop();
    clearConversation();
    addMessageBubble(ChatRole::Assistant, QStringLiteral("New chat started. What would you like to talk about?"));
    m_statusLabel->setText(QStringLiteral("Ready"));
}

void MainWindow::onStopClicked()
{
    m_client->abortRequest();
    m_piper->stop();
    setGenerating(false);
    m_statusLabel->setText(QStringLiteral("Stopped"));
}

void MainWindow::onResponseChunk(const QString &text)
{
    m_streamBuffer.append(text);
    if (m_activeAssistantBubble) {
        m_activeAssistantBubble->appendText(text);
        scrollToBottom();
    }
}

void MainWindow::onResponseFinished()
{
    const QString spoken = ChatBubble::spokenText(m_streamBuffer);
    if (!m_streamBuffer.isEmpty()) {
        QJsonObject assistantMessage;
        assistantMessage.insert(QStringLiteral("role"), QStringLiteral("assistant"));
        assistantMessage.insert(QStringLiteral("content"), m_streamBuffer);
        m_messages.append(assistantMessage);
    }

    m_activeAssistantBubble = nullptr;
    m_streamBuffer.clear();
    setGenerating(false);
    m_statusLabel->setText(QStringLiteral("Ready"));

    if (m_listenButton->isChecked() && !m_speech->isRecording() && !spoken.isEmpty()) {
        m_piper->speak(spoken);
    }
    scrollToBottom();
}

void MainWindow::onClientError(const QString &message)
{
    if (m_activeAssistantBubble) {
        m_activeAssistantBubble->appendText(QStringLiteral("\n\n[Error: %1]").arg(message));
    } else {
        addMessageBubble(ChatRole::Assistant,
                         QStringLiteral("Could not reach Ollama. Is it running?\n\n%1").arg(message));
    }

    m_activeAssistantBubble = nullptr;
    m_streamBuffer.clear();
    setGenerating(false);
    m_statusLabel->setText(QStringLiteral("Error"));
}

void MainWindow::setGenerating(bool generating)
{
    m_sendButton->setDisabled(generating);
    m_modelCombo->setDisabled(generating);
    m_wordLimitCombo->setDisabled(generating);
    m_recordButton->setDisabled(generating || m_speech->isBusy());
    m_micCombo->setDisabled(generating || m_speech->isRecording());
    m_speakerCombo->setDisabled(generating || m_piper->isBusy());
    m_refreshMicsButton->setDisabled(generating || m_speech->isRecording());
    m_stopButton->setText(QStringLiteral("Stop generating"));
    m_stopButton->setVisible(generating || m_piper->isBusy());
    if (!m_speech->isBusy()) {
        m_statusLabel->setText(generating ? QStringLiteral("Generating…") : QStringLiteral("Ready"));
    }
}

void MainWindow::refreshMicrophones()
{
    const QByteArray previousId = m_micCombo->currentData().toByteArray();
    m_micCombo->blockSignals(true);
    m_micCombo->clear();

    const QList<QAudioDevice> devices = m_speech->microphones();
    for (const QAudioDevice &device : devices) {
        m_micCombo->addItem(device.description(), device.id());
    }

    int index = m_micCombo->findData(previousId);
    if (index < 0) {
        index = 0;
    }
    if (m_micCombo->count() == 0) {
        m_micCombo->addItem(QStringLiteral("No microphone found"));
        m_recordButton->setEnabled(false);
    } else {
        m_micCombo->setCurrentIndex(index);
        m_recordButton->setEnabled(!m_client->isBusy());
        onMicSelectionChanged(index);
    }
    m_micCombo->blockSignals(false);
}

void MainWindow::refreshSpeakers()
{
    const QByteArray previousId = m_speakerCombo->currentData().toByteArray();
    m_speakerCombo->blockSignals(true);
    m_speakerCombo->clear();

    const QList<QAudioDevice> devices = m_piper->speakers();
    for (const QAudioDevice &device : devices) {
        m_speakerCombo->addItem(device.description(), device.id());
    }

    int index = m_speakerCombo->findData(previousId);
    if (index < 0) {
        index = 0;
    }
    if (m_speakerCombo->count() == 0) {
        m_speakerCombo->addItem(QStringLiteral("No speaker found"));
    } else {
        m_speakerCombo->setCurrentIndex(index);
        onSpeakerSelectionChanged(index);
    }
    m_speakerCombo->blockSignals(false);
}

void MainWindow::onMicSelectionChanged(int index)
{
    if (index < 0) {
        return;
    }
    const QByteArray id = m_micCombo->itemData(index).toByteArray();
    const QList<QAudioDevice> devices = m_speech->microphones();
    for (const QAudioDevice &device : devices) {
        if (device.id() == id) {
            m_speech->setMicrophone(device);
            return;
        }
    }
}

void MainWindow::onSpeakerSelectionChanged(int index)
{
    if (index < 0) {
        return;
    }
    const QByteArray id = m_speakerCombo->itemData(index).toByteArray();
    const QList<QAudioDevice> devices = m_piper->speakers();
    for (const QAudioDevice &device : devices) {
        if (device.id() == id) {
            m_piper->setSpeaker(device);
            return;
        }
    }
}

void MainWindow::onRefreshMicsClicked()
{
    refreshMicrophones();
    refreshSpeakers();
    m_statusLabel->setText(QStringLiteral("Microphones and speakers refreshed"));
}

void MainWindow::onRecordClicked()
{
    if (m_speech->isRecording()) {
        m_speech->stopRecording();
        return;
    }
    if (m_client->isBusy() || m_speech->isBusy()) {
        return;
    }
    onMicSelectionChanged(m_micCombo->currentIndex());
    m_piper->stop();
    m_speechPrefix = m_input->toPlainText().trimmed();
    m_speech->startRecording();
}

void MainWindow::showSpeechText(const QString &text)
{
    const QString combined = m_speechPrefix.isEmpty() ? text : m_speechPrefix + QLatin1Char(' ') + text;
    m_input->setPlainText(combined);
    m_input->moveCursor(QTextCursor::End);
}

void MainWindow::onPartialTranscription(const QString &text)
{
    showSpeechText(text);
}

void MainWindow::onTranscriptionReady(const QString &text)
{
    showSpeechText(text);
    m_input->setFocus();
    m_statusLabel->setText(QStringLiteral("Ready"));
    m_recordButton->setEnabled(!m_client->isBusy());
    m_sendButton->setEnabled(!m_client->isBusy());
}

void MainWindow::onRecordingChanged(bool recording)
{
    m_recordButton->setText(recording ? QStringLiteral("Stop") : QStringLiteral("Mic"));
    m_recordButton->setProperty("recording", recording);
    m_recordButton->style()->unpolish(m_recordButton);
    m_recordButton->style()->polish(m_recordButton);
    m_micCombo->setDisabled(recording || m_client->isBusy());
    m_refreshMicsButton->setDisabled(recording || m_client->isBusy());
    m_sendButton->setDisabled(recording || m_client->isBusy());
}

void MainWindow::onSpeechError(const QString &message)
{
    m_statusLabel->setText(QStringLiteral("Speech error"));
    addMessageBubble(ChatRole::Assistant, message);
    m_recordButton->setEnabled(!m_client->isBusy());
    m_sendButton->setEnabled(!m_client->isBusy() && !m_speech->isRecording());
}

void MainWindow::onPiperSpeakingChanged(bool speaking)
{
    m_speakerCombo->setDisabled(speaking || m_client->isBusy());
    if (speaking) {
        m_stopButton->setText(QStringLiteral("Stop speaking"));
        m_stopButton->setVisible(true);
        return;
    }
    if (!m_client->isBusy()) {
        m_stopButton->setVisible(false);
        m_stopButton->setText(QStringLiteral("Stop generating"));
    }
}

void MainWindow::onPiperStatus(const QString &status)
{
    if (!m_client->isBusy() && !m_speech->isBusy()) {
        m_statusLabel->setText(status);
    }
}

void MainWindow::onPiperError(const QString &message)
{
    m_statusLabel->setText(QStringLiteral("Speech error"));
    addMessageBubble(ChatRole::Assistant, message);
}

void MainWindow::onSpeechStatus(const QString &status)
{
    m_statusLabel->setText(status);
    const bool locked = status == QStringLiteral("Transcribing…") || m_speech->isRecording() || m_client->isBusy();
    m_recordButton->setDisabled(m_client->isBusy() || status == QStringLiteral("Transcribing…"));
    m_sendButton->setDisabled(locked);
}

void MainWindow::scrollToBottom()
{
    if (!m_chatScroll || !m_chatScroll->verticalScrollBar()) {
        return;
    }
    QScrollBar *bar = m_chatScroll->verticalScrollBar();
    bar->setValue(bar->maximum());
}

ChatBubble *MainWindow::addMessageBubble(ChatRole role, const QString &text)
{
    auto *bubble = new ChatBubble(role, text, m_messageContainer);
    const int insertIndex = qMax(0, m_messageLayout->count() - 1);
    m_messageLayout->insertWidget(insertIndex, bubble);
    scrollToBottom();
    return bubble;
}

void MainWindow::clearConversation()
{
    m_messages = QJsonArray();
    m_activeAssistantBubble = nullptr;
    m_streamBuffer.clear();

    while (m_messageLayout->count() > 1) {
        QLayoutItem *item = m_messageLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

int MainWindow::currentWordLimit() const
{
    bool ok = false;
    const int limit = m_wordLimitCombo->currentText().toInt(&ok);
    return ok ? limit : 75;
}

QJsonArray MainWindow::conversationForApi() const
{
    QJsonArray messages;

    QJsonObject systemMessage;
    systemMessage.insert(QStringLiteral("role"), QStringLiteral("system"));
    systemMessage.insert(
        QStringLiteral("content"),
        QStringLiteral("Keep every reply within %1 words. Answer directly from your own knowledge. "
                       "Do not call tools, do not search the web, and do not output tool_code, XML, "
                       "or code. If you are unsure, say so in one sentence.")
            .arg(currentWordLimit()));
    messages.append(systemMessage);

    for (const QJsonValue &value : m_messages) {
        messages.append(value);
    }
    return messages;
}
