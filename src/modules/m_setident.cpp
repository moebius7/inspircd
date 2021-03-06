/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/** Handle /SETIDENT
 */
class CommandSetident : public Command
{
 public:
 CommandSetident(Module* Creator) : Command(Creator,"SETIDENT", 1)
	{
		allow_empty_last_param = false;
		flags_needed = 'o'; syntax = "<new-ident>";
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		if (parameters[0].size() > ServerInstance->Config->Limits.IdentMax)
		{
			user->WriteNotice("*** SETIDENT: Ident is too long");
			return CMD_FAILURE;
		}

		if (!ServerInstance->IsIdent(parameters[0].c_str()))
		{
			user->WriteNotice("*** SETIDENT: Invalid characters in ident");
			return CMD_FAILURE;
		}

		user->ChangeIdent(parameters[0].c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "%s used SETIDENT to change their ident to '%s'", user->nick.c_str(), user->ident.c_str());

		return CMD_SUCCESS;
	}
};

class ModuleSetIdent : public Module
{
	CommandSetident cmd;

 public:
	ModuleSetIdent() : cmd(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for the SETIDENT command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSetIdent)
