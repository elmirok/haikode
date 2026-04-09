/*
 * Copyright The Genio Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Based on TrackGit (https://github.com/HaikuArchives/TrackGit)
 * Original author: Hrishikesh Hiraskar 
 * Copyright Hrishikesh Hiraskar and other HaikuArchives contributors (see GitHub repo for details)
 */

#pragma once


#include <functional>
#include <vector>

#include "GException.h"
#include "Log.h"
#include "Messenger.h"

class BLooper;
class BPath;


// TODO: copied from libgit2's types.h. Remove
typedef struct git_repository git_repository;

namespace Genio::Git {

	const int CANCEL_CREDENTIALS = -123;

	const size_t MAX_ELEMENTS = 1000;

	enum PullResult {
		UpToDate,
		FastForwarded,
		Merged
	};

	struct callback_data {
		BMessenger messenger;
		uint32 what;
	};

	// Git exceptions

	class GitException : public GException {
	public:
		GitException(int error, BString const& message)
			:
			GException(error, BString(message).Prepend("Git: "))
		{
			// TODO: Move away from here and into the catcher
			// otherwise it's harder to track where it happened
			LogError("GitException %s, error = %d" , message.String(), error);
		}
	};


	class GitConflictException : public GitException {
	public:
		GitConflictException(int error, BString const& message,
			const std::vector<BString>& files)
			:
			GitException(error, message),
			fFiles(files)
		{
			// TODO: Move away from here and into the catcher
			// otherwise it's harder to track where it happened
			LogError("GitConflictException %s, error = %d, files = %d",
				message.String(), error, files.size());
		}

		std::vector<BString> GetFiles() const noexcept { return fFiles; }

	private:
		std::vector<BString> fFiles;
	};


	class GitRepository {
	public:
		typedef std::vector<std::pair<BString, BString>> RepoFiles;

		enum branch_type {
			BRANCH_LOCAL = 1,
			BRANCH_REMOTE = 2,
			BRANCH_ALL = BRANCH_LOCAL | BRANCH_REMOTE
		};


										GitRepository(const BString path);
										~GitRepository();

		static BPath					Clone(const BString url, const BPath localPath,
												callback_data* progressData);

		static bool						IsValid(const BString path);
		bool							IsInitialized();
		void							Init(bool createInitalCommit = true);

		std::vector<BString>			GetTags(size_t maxTags = MAX_ELEMENTS) const;

		std::vector<BString>			GetBranches(branch_type type = BRANCH_LOCAL, size_t maxBranches = MAX_ELEMENTS) const;
		int								SwitchBranch(const BString branch);
		BString							GetCurrentBranch() const;
		void							DeleteBranch(const BString branch, branch_type type);
		void							RenameBranch(const BString oldName, const BString newName,
											branch_type type);
		void							CreateBranch(const BString existingBranchName,
											branch_type type, const BString newBranchName);

		void							Fetch(bool prune = false);
		void							Merge(const BString source, const BString dest);
		PullResult						Pull(const BString branchName);
		void 							PullRebase();
		void 							Push();

		void 							StashSave(const BString message);
		void 							StashPop();
		void 							StashApply();

		RepoFiles						GetFiles() const;

		static BLooper*					Looper();

		static int 						check(int status,
											std::function<void(void)> execute_on_fail = nullptr,
											std::function<bool(const int)> custom_checker = nullptr);

	private:
		git_repository 					*fRepository;
		BString							fRepositoryPath;
		mutable BString					fCurrentBranch;
		bool							fInitialized;

		void							_Open();

		void							_CreateInitialCommit();

		void							_NotifyBranchChanged() const;
	};
}
