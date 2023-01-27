/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2023 Raden Solutions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.ui.eclipse.console;

import org.eclipse.rap.ui.resources.IResource;

/**
 * Resource for message proxy widget JavaScript
 */
public class MsgProxyWidgetScript implements IResource
{
   /**
    * @see org.eclipse.rap.ui.resources.IResource#getLoader()
    */
   @Override
   public ClassLoader getLoader()
   {
      return getClass().getClassLoader();
   }

   /**
    * @see org.eclipse.rap.ui.resources.IResource#getLocation()
    */
   @Override
   public String getLocation()
   {
      return "js/msgproxy.js";
   }

   /* (non-Javadoc)
    * @see org.eclipse.rap.ui.resources.IResource#isJSLibrary()
    */
   @Override
   public boolean isJSLibrary()
   {
      return true;
   }

   /**
    * @see org.eclipse.rap.ui.resources.IResource#isExternal()
    */
   @Override
   public boolean isExternal()
   {
      return false;
   }
}
