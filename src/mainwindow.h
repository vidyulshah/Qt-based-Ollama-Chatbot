#pragma once

#include <QJsonArray>
#include <QMainWindow>

#include "chatbubble.h"
class OllamaClient;
class PiperSpeaker;
class SpeechInput;
class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QVBoxLayout;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSendClicked();
    void onNewChatClicked();
    void onStopClicked();
    void onResponseChunk(const QString &text);
    void onResponseFinished();
    void onClientError(const QString &message);
    void onMicSelectionChanged(int index);
    void onSpeakerSelectionChanged(int index);
    void onRefreshMicsClicked();
    void onRecordClicked();
    void onRecordingChanged(bool recording);
    void onPartialTranscription(const QString &text);
    void onTranscriptionReady(const QString &text);
    void onSpeechError(const QString &message);
    void onSpeechStatus(const QString &status);
    void onPiperSpeakingChanged(bool speaking);
    void onPiperStatus(const QString &status);
    void onPiperError(const QString &message);

private:
    void buildUi();
    void setGenerating(bool generating);
    void scrollToBottom();
    ChatBubble *addMessageBubble(ChatRole role, const QString &text);
    void clearConversation();
    void refreshMicrophones();
    void refreshSpeakers();
    void showSpeechText(const QString &text);
    int currentWordLimit() const;
    QJsonArray conversationForApi() const;

    OllamaClient *m_client = nullptr;
    SpeechInput *m_speech = nullptr;
    PiperSpeaker *m_piper = nullptr;

    QComboBox *m_modelCombo = nullptr;
    QComboBox *m_wordLimitCombo = nullptr;
    QComboBox *m_micCombo = nullptr;
    QComboBox *m_speakerCombo = nullptr;
    QLabel *m_statusLabel = nullptr;
    QScrollArea *m_chatScroll = nullptr;
    QWidget *m_messageContainer = nullptr;
    QVBoxLayout *m_messageLayout = nullptr;
    QTextEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_listenButton = nullptr;
    QPushButton *m_recordButton = nullptr;
    QPushButton *m_refreshMicsButton = nullptr;

    ChatBubble *m_activeAssistantBubble = nullptr;
    QJsonArray m_messages;
    QString m_streamBuffer;
    QString m_speechPrefix;
};
