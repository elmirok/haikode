/*
 * Copyright The Genio Contributors
 * Copyright Hrishikesh Hiraskar 
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <cstring>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <Button.h>
#include <Catalog.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <Path.h>
#include <TextControl.h>

#include "GitCredentialsWindow.h"
#include "GitRepository.h"
#include "Utils.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "GitCredentialsWindow"


#define MAX_AUTHENTICATION_ATTEMPTS 16
int32 GitCredentialsWindow::sAuthenticationAttempts = 0;


GitCredentialsWindow::GitCredentialsWindow(const char* title, bool username, bool password)
	:
	BWindow(BRect(0, 0, 300, 150), title,
			B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE
			| B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_CLOSABLE),
	fUsername(nullptr),
	fPassword(nullptr),
	fUsernameString(nullptr),
	fPasswordString(nullptr)
{
	fUsername = new BTextControl(B_TRANSLATE("Username:"), "", NULL);
	if (password) {
		fPassword = new BTextControl(B_TRANSLATE("Password:"), "", NULL);
		fPassword->TextView()->HideTyping(true);
	}
	BButton* fOK = new BButton("ok", B_TRANSLATE("OK"),
			new BMessage(kCredOK));
	BButton* fCancel = new BButton("cancel", B_TRANSLATE("Cancel"),
			new BMessage(kCredCancel));

	if (password) {
		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_INSETS)
			.AddGrid()
				.AddTextControl(fUsername, 0, 0)
				.AddTextControl(fPassword, 0, 1)
			.End()
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(fCancel)
				.Add(fOK)
			.End();
	} else {
		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_INSETS)
			.AddGrid()
				.AddTextControl(fUsername, 0, 0)
			.End()
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(fCancel)
				.Add(fOK)
			.End();
	}
	CenterOnScreen();
	Show();
}


void
GitCredentialsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kCredOK:
			fUsernameString->SetTo(fUsername->Text());
			if (fPassword != nullptr && fPasswordString != nullptr)
				fPasswordString->SetTo(fPassword->Text());
			Quit();
			break;
		case kCredCancel:
			fUsernameString->SetTo("");
			if (fPassword != nullptr && fPasswordString != nullptr)
				fPasswordString->SetTo("");
			Quit();
			break;
		default:
			BWindow::MessageReceived(message);
			break;
	}
}


/* static */
thread_id
GitCredentialsWindow::OpenCredentialsWindow(const char* title, BString& username)
{
	GitCredentialsWindow* window = new GitCredentialsWindow(title, true);
	window->fUsernameString = &username;
	return window->Thread();
}


/* static */
thread_id
GitCredentialsWindow::OpenCredentialsWindow(const char* title, BString& username,
													BString& password)
{
	GitCredentialsWindow* window = new GitCredentialsWindow(title, true, true);
	window->fUsernameString = &username;
	window->fPasswordString = &password;
	return window->Thread();
}


int
GitCredentialsWindow::authentication_callback(git_credential** out, const char* url,
									const char* username_from_url,
									unsigned int allowed_types,
									void* payload)
{
	// Protect from infinite loop which libgit does by design
	if (++sAuthenticationAttempts > MAX_AUTHENTICATION_ATTEMPTS) {
		sAuthenticationAttempts = 0;
		LogTrace("GitCredentialsWindow: authentication attempts limits reached!");
		return Genio::Git::CANCEL_CREDENTIALS;
	}

	if (Logger::IsTraceEnabled()) {
		LogTrace("authentication_callback: allowed types (%x): ", allowed_types);
		if (allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT)
			LogTrace("\tGIT_CREDENTIAL_USERPASS_PLAINTEXT");
		if (allowed_types & GIT_CREDENTIAL_SSH_KEY)
			LogTrace("\tGIT_CREDENTIAL_SSH_KEY");
		if (allowed_types & GIT_CREDENTIAL_SSH_CUSTOM)
			LogTrace("\tGIT_CREDENTIAL_SSH_CUSTOM");
		if (allowed_types & GIT_CREDENTIAL_DEFAULT)
			LogTrace("\tGIT_CREDENTIAL_DEFAULT");
		if (allowed_types & GIT_CREDENTIAL_SSH_INTERACTIVE)
			LogTrace("\tGIT_CREDENTIAL_SSH_INTERACTIVE");
		if (allowed_types & GIT_CREDENTIAL_USERNAME)
			LogTrace("\tGIT_CREDENTIAL_USERNAME");
		if (allowed_types & GIT_CREDENTIAL_SSH_MEMORY)
			LogTrace("\tGIT_CREDENTIAL_SSH_MEMORY");
	}

	BString username;
	if (allowed_types & GIT_CREDENTIAL_USERNAME) {
		// Ask for username
		thread_id thread = OpenCredentialsWindow(B_TRANSLATE("Git - Username"), username);
		status_t winStatus = B_OK;
		wait_for_thread(thread, &winStatus);
		if (!username.IsEmpty())
			return git_credential_username_new(out, username);
		return Genio::Git::CANCEL_CREDENTIALS;
	}
	// TODO: this only works when using an ssh_agent.
	// Allow user to specify the ssh key and eventually the ssh key passkey
	if (allowed_types & GIT_CREDENTIAL_SSH_KEY) {
		username = username_from_url;
		if (username.IsEmpty()) {
			// If no user specified, use "git"
			username = "git";
		}
		int gitStatus = git_credential_ssh_key_from_agent(out, username);
		if (gitStatus == GIT_OK) {
			// It seems libgit2 always returns GIT_OK here...
			return GIT_OK;
		}

		// TODO: ask user for ssh key instead
	}

	BString password;
	thread_id thread = OpenCredentialsWindow(B_TRANSLATE("Git - User Credentials"),
											username, password);
	status_t winStatus = B_OK;
	wait_for_thread(thread, &winStatus);

	int error = 0;
	if (!username.IsEmpty() && !password.IsEmpty()) {
		error = git_cred_userpass_plaintext_new(out, username, password);
	}

	/**
	 * If user cancels the credentials prompt, the username is empty.
	 * Cancel the command in such case.
	 */
	if (username.IsEmpty())
		return Genio::Git::CANCEL_CREDENTIALS;

	return error;
}


static bool
verify_host_key(const char *host, const git_cert_hostkey* sshKeyCert)
{
	BPath knownHostsPath;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &knownHostsPath) != B_OK)
		return false;

	knownHostsPath.Append("ssh/known_hosts");
	if (!BEntry(knownHostsPath.Path()).Exists())
		return false;

	std::ifstream knownHostsFile(knownHostsPath.Path());
	std::string line;
	while (std::getline(knownHostsFile, line)) {
		// Skip comments and empty lines
		if (line.empty() || line[0] == '#')
			continue;

		std::istringstream iss(line);
		std::string hostname;
		std::string keytype;
		std::string publicKeyBase64;
		if (!(iss >> hostname >> keytype >> publicKeyBase64))
			continue;

		if (hostname != host)
			continue;

		// Compare with existing key
		if (keytype == "ecdsa-sha2-nistp256" || keytype == "ssh-rsa" || keytype == "ssh-ed25519") {
			unsigned char* decoded = nullptr;
			size_t decodedLength = 0;
			Base64Decode(publicKeyBase64, decoded, decodedLength);

			// TODO: check against sha1 or md5 (deprecated) if we only have those
			std::string hash = SHA256Hash(decoded, decodedLength);
			// Compare with received host sha256
			if (memcmp(hash.c_str(), sshKeyCert->hash_sha256, 32) == 0) {
				delete[] decoded;
				return true;
			}
			delete[] decoded;
		}
	}

	return false;
}


/* static */
int
GitCredentialsWindow::certificate_check_callback(git_cert *cert, int valid,
	const char *host, void *payload)
{
	// Only handle HOSTKEYS
	if (valid || cert->cert_type != GIT_CERT_HOSTKEY_LIBSSH2) {
		// Let the library handle this
		return 1;
	}

	git_cert_hostkey* sshKeyCert = reinterpret_cast<git_cert_hostkey*>(cert);

	// Check the known_hosts
	if (verify_host_key(host, sshKeyCert))
		return 1;

	BString message = B_TRANSLATE("Accept ssh host key from \%host\% ?");
	message.Append("\n");
	if (sshKeyCert->type & GIT_CERT_SSH_MD5) {
		BString hashMD5;
		for (size_t i = 0; i < 16; i++) {
			char hex[3];
			sprintf(hex, "%02x", sshKeyCert->hash_md5[i]);
			hashMD5.Append(hex);
		}
		message.Append("\nMD5: ");
		message.Append(hashMD5);
		LogTrace("md5: %s\n", hashMD5.String());
	}
	if (sshKeyCert->type & GIT_CERT_SSH_SHA1) {
		BString hashSHA1;
		for (size_t i = 0; i < 20; i++) {
			char hex[3];
			sprintf(hex, "%02x", sshKeyCert->hash_sha1[i]);
			hashSHA1.Append(hex);
		}
		message.Append("\nSHA1: ");
		message.Append(hashSHA1);
		LogTrace("sha1: %s\n", hashSHA1.String());
	}
	if (sshKeyCert->type & GIT_CERT_SSH_SHA256) {
		BString hashSHA256;
		for (size_t i = 0; i < 32; i++) {
			char hex[3];
			sprintf(hex, "%02x", sshKeyCert->hash_sha256[i]);
			hashSHA256.Append(hex);
		}
		message.Append("\nSHA256: ");
		message.Append(hashSHA256);
		LogTrace("sha256: %s\n", hashSHA256.String());
	}
	if (sshKeyCert->type & GIT_CERT_SSH_RAW) {
		BString hashRAW;
		//printf("GIT_CERT_SSH_RAW: len %lu\n", sshKeyCert->hostkey_len);
	}

	message.ReplaceAll("\%host\%", host);
	BAlert* alert = new BAlert("", message.String(),
		B_TRANSLATE("Yes"), B_TRANSLATE("No"), nullptr,
		B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->TextView()->SetWordWrap(false);
	int result = alert->Go();
	// TODO: Cache the result by adding the key to known_hosts ?
	if (result == 0)
		return 0;

	return -1;
}
