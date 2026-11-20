/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

/**
 * Provide/inject key for sharing the dashboard data context (snapshots,
 * loading state, refresh function) from DashboardGrid down to all widgets
 * without prop-drilling.
 */
 import { inject, isRef, provide } from 'vue'

 export const DASHBOARD_CONTEXT_KEY = Symbol('dashboardContext')

 function assertDashboardContext(context) {
   if (!isRef(context?.activeDirectors) || !isRef(context?.refreshToken)) {
     throw new Error(
       'Dashboard widgets require activeDirectors and refreshToken refs from provideDashboardContext().'
     )
   }
   return context
 }

 export function provideDashboardContext(context) {
   const validatedContext = assertDashboardContext(context)
   provide(DASHBOARD_CONTEXT_KEY, validatedContext)
   return validatedContext
 }

 export function useDashboardContext() {
   return assertDashboardContext(inject(DASHBOARD_CONTEXT_KEY))
 }
