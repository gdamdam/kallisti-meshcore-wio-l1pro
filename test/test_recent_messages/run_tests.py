#!/usr/bin/env python3
"""Host tests of the buffer and actual OLED screen without firmware dependencies.
The screen remains in UITask.cpp to minimize upstream file churn. Extract its
class verbatim into a temporary fixture; substitute only hardware/task adapters.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'examples/companion_radio/ui-new/UITask.cpp').read_text()
abstract = (root / 'examples/companion_radio/AbstractUITask.h').read_text()
metadata = abstract.split('struct UIMessageMetadata {', 1)[1].split('\n};', 1)[0]
screen = source.split('class InboxScreen : public UIScreen {', 1)[1].split('\n#else\nclass MsgPreviewScreen', 1)[0]
task_methods = source.split('void UITask::gotoInbox() {', 1)[1].split('\n#else\nvoid UITask::newMsg', 1)[0]
sync_method = source.split('void UITask::msgRead(int msgcount) {', 1)[1].split('\n#if defined(UI_RECENT_INBOX)\nbool UITask::inboxReplyTarget', 1)[0]
wake_method = source.split('char UITask::checkDisplayOn(char c) {', 1)[1].split('\nchar UITask::handleLongPress', 1)[0]
with tempfile.TemporaryDirectory(prefix='wio-inbox-test-') as temporary:
    temp = Path(temporary)
    (temp / 'screen-under-test.h').write_text(
        'struct UIMessageMetadata {' + metadata + '\n};\n'
        'class InboxScreen : public UIScreen {' + screen +
        'void UITask::gotoInbox() {' + task_methods +
        'void UITask::msgRead(int msgcount) {' + sync_method +
        'char UITask::checkDisplayOn(char c) {' + wake_method)
    target = abstract.split('struct UISendTarget {', 1)[1].split('\n};', 1)[0]
    (temp / 'send-target-under-test.h').write_text('struct UISendTarget {' + target + '\n};\n')
    home = source.split('class HomeScreen : public UIScreen {', 1)[1].split('class InboxScreen', 1)[0]
    enum = home.split('enum HomePage {', 1)[1].split('\n  };', 1)[0]
    indicators = home.split('// curr page indicator', 1)[1].split('if (_page == HomePage::FIRST)', 1)[0]
    auto_read = home.split('  bool canAutoRead()',1)[1].split('#endif',1)[0]
    handler = home.split('bool handleInput(char c) override {', 1)[1].split('\n  }\n};', 1)[0]
    (temp / 'home-nav-under-test.h').write_text('class HomeNav : public UIScreen { public: enum HomePage {' + enum + '\n};\nUITask* _task; ActionsMenu _actions; uint8_t _page = FIRST; bool _shutdown_init = false; HomeNav(UITask* task) : _task(task), _actions(task) {} int render(DisplayDriver& display) override { ' + indicators + ' return 0; } bool canAutoRead()' + auto_read + ' bool handleInput(char c) override {' + handler + '\n} };\n')
    auto_entry = source.split('// Process control events before considering an automatic screen change.',1)[1].split('#endif',1)[0]
    (temp / 'auto-entry-under-test.h').write_text('void UITask::autoEntry(char c) {' + auto_entry.replace('HomeScreen', 'HomeNav') + '}\n')
    mesh_source = (root / 'examples/companion_radio/MyMesh.cpp').read_text()
    ack = mesh_source.split('ContactInfo*  MyMesh::processAck(', 1)[1].split('\nvoid MyMesh::queueMessage', 1)[0]
    (temp / 'reply-routing-under-test.h').write_text('ContactInfo* MyMesh::processAck(' + ack + mesh_source.split('// UI replies use full identities, never sender names or short relay prefixes.', 1)[1].rsplit('\n#endif', 1)[0])
    storage_source = (root / 'examples/companion_radio/DataStore.cpp').read_text()
    (temp / 'channel-storage-under-test.h').write_text('bool DataStore::saveUIChannels(' + storage_source.split('bool DataStore::saveUIChannels(',1)[1].split('\n#endif',1)[0])
    (temp / 'Arduino.h').write_text('#pragma once\n#define HIGH 1\n#define LOW 0\n#define INPUT 0\n#define INPUT_PULLUP 1\n#define INPUT_PULLDOWN 2\n#include <stdint.h>\nextern unsigned long button_millis;\nextern int button_level;\ninline unsigned long millis() { return button_millis; }\ninline int digitalRead(int) { return button_level; }\ninline int analogRead(int) { return button_level; }\ninline void pinMode(int, int) { }\n')
    for test in ('recent_messages', 'inbox_screen', 'buttons', 'persistence', 'quick_replies', 'reply_routing', 'compose_state', 'editor', 'channel_storage'):
        binary = temp / test
        subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-isystem', str(root / 'src'),
                        '-I' + str(temp), str(Path(__file__).parent / (test + '.cpp')),
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
