/*
 * Copyright (c) 2020, the SerenityOS developers.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AutoCompleteResponse.h"
#include <AK/Forward.h>
#include <AK/LexicalPath.h>
#include <AK/Types.h>
#include <AK/WeakPtr.h>
#include <AK/Weakable.h>
#include <LibIPC/ServerConnection.h>

#include <DevTools/HackStudio/LanguageServers/LanguageClientEndpoint.h>
#include <DevTools/HackStudio/LanguageServers/LanguageServerEndpoint.h>

namespace HackStudio {

class LanguageClient;

class ServerConnection
    : public IPC::ServerConnection<LanguageClientEndpoint, LanguageServerEndpoint>
    , public LanguageClientEndpoint {
public:
    ServerConnection(const StringView& socket)
        : IPC::ServerConnection<LanguageClientEndpoint, LanguageServerEndpoint>(*this, socket)
    {
    }

    void attach(LanguageClient& client)
    {
        m_language_client = &client;
    }

    void detach()
    {
        m_language_client = nullptr;
    }

    virtual void handshake() override
    {
        auto response = send_sync<Messages::LanguageServer::Greet>();
        set_my_client_id(response->client_id());
    }

    WeakPtr<LanguageClient> language_client() { return m_language_client; }

    template<typename ConcreteType>
    static NonnullRefPtr<ServerConnection> get_or_create(const String& project_path)
    {
        static HashMap<String, NonnullRefPtr<ConcreteType>> s_instances_for_projects;
        auto key = LexicalPath { project_path }.string();
        if (auto instance = s_instances_for_projects.get(key); instance.has_value())
            return *instance.value();

        auto connection = ConcreteType::construct();
        connection->handshake();
        s_instances_for_projects.set(key, *connection);
        return *connection;
    }

protected:
    virtual void handle(const Messages::LanguageClient::AutoCompleteSuggestions&) override;

    WeakPtr<LanguageClient> m_language_client;
};

class LanguageClient : public Weakable<LanguageClient> {
public:
    explicit LanguageClient(NonnullRefPtr<ServerConnection>&& connection)
        : m_connection(*connection)
        , m_server_connection(move(connection))
    {
        m_previous_client = m_connection.language_client();
        m_connection.attach(*this);
    }

    virtual ~LanguageClient()
    {
        m_connection.detach();
        if (m_previous_client)
            m_connection.attach(*m_previous_client);
    }

    virtual void open_file(const String& path, int fd);
    virtual void set_file_content(const String& path, const String& content);
    virtual void insert_text(const String& path, const String& text, size_t line, size_t column);
    virtual void remove_text(const String& path, size_t from_line, size_t from_column, size_t to_line, size_t to_column);
    virtual void request_autocomplete(const String& path, size_t cursor_line, size_t cursor_column);
    virtual void set_autocomplete_mode(const String& mode);

    void provide_autocomplete_suggestions(const Vector<GUI::AutocompleteProvider::Entry>&);

    Function<void(Vector<GUI::AutocompleteProvider::Entry>)> on_autocomplete_suggestions;

private:
    ServerConnection& m_connection;
    NonnullRefPtr<ServerConnection> m_server_connection;
    WeakPtr<LanguageClient> m_previous_client;
};

template<typename ServerConnectionT>
static inline NonnullOwnPtr<LanguageClient> get_language_client(const String& project_path)
{
    return make<LanguageClient>(ServerConnection::get_or_create<ServerConnectionT>(project_path));
}

}
