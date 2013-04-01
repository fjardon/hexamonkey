//This file is part of the HexaMonkey project, a multimedia analyser
//Copyright (C) 2013  Sevan Drapeau-Martin, Nicolas Fleury

//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either version 2
//of the License, or (at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "localscope.h"

#include "variant.h"
#include "holder.h"

LocalScope::LocalScope(Holder &holder)
    : holder(holder)
{
}

void LocalScope::clear()
{
    _map.clear();
}

Variant* LocalScope::doDeclare(const Variant &key) const
{
    Variant* value = &holder.getNew();
    _map.insert(std::make_pair(key.toString(), value));
    return value;
}

Variant *LocalScope::doGet(const Variant &key) const
{
    if(key.canConvertTo(Variant::string))
    {
        auto it = _map.find(key.toString());
        if(it != _map.end())
        {
            return it->second;
        }
    }
    return nullptr;
}