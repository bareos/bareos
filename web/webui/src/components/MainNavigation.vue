<template>
  <span>
    <v-navigation-drawer
      v-model="drawer"
      fixed
      app
      dark

    >
      <v-list>

        <v-list-tile href="/">
          <v-list-tile-action>
            <v-icon>dashboard</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Dashboard</v-list-tile-title>
          </v-list-tile-content>
        </v-list-tile>

        <v-list-group
        prepend-icon="archive"
        value="true"
        >
          <v-list-tile slot="activator">
            <v-list-tile-title>Backup</v-list-tile-title>
          </v-list-tile>
          <v-list-tile href="/backup/history">
          <v-list-tile-action>
            <v-icon>history</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>History</v-list-tile-title>
          </v-list-tile-content>
          </v-list-tile>
          <v-list-tile href="/backup/clients">
          <v-list-tile-action>
            <v-icon>computer</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Clients</v-list-tile-title>
          </v-list-tile-content>
          </v-list-tile>
          <v-list-tile href="/backup/jobs">
          <v-list-tile-action>
            <v-icon>assignment</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Jobs</v-list-tile-title>
          </v-list-tile-content>
          </v-list-tile>
          <v-list-tile href="/backup/filesets">
          <v-list-tile-action>
            <v-icon>description</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Filesets</v-list-tile-title>
          </v-list-tile-content>
          </v-list-tile>
          <v-list-tile href="/backup/schedules">
          <v-list-tile-action>
            <v-icon>schedule</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Schedules</v-list-tile-title>
          </v-list-tile-content>
          </v-list-tile>
        </v-list-group>
        <v-list-tile href="/restore/">
          <v-list-tile-action>
            <v-icon>unarchive</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Restore</v-list-tile-title>
          </v-list-tile-content>
        </v-list-tile>
        <v-list-tile href="/notifications/">
          <v-list-tile-action>
            <v-icon>notifications</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Notifications</v-list-tile-title>
          </v-list-tile-content>
        </v-list-tile>
        <v-list-tile href="/console/">
          <v-list-tile-action>
            <v-icon>keyboard</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Console</v-list-tile-title>
          </v-list-tile-content>
        </v-list-tile>

        <v-list-group
        prepend-icon="settings"
        value="true"
        >
          <v-list-tile slot="activator">
            <v-list-tile-title>Settings</v-list-tile-title>
          </v-list-tile>
          <v-list-tile href="/settings/users/">
          <v-list-tile-action>
            <v-icon>person</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Users</v-list-tile-title>
          </v-list-tile-content>
          </v-list-tile>
          <v-list-tile href="/settings/Groups/">
          <v-list-tile-action>
            <v-icon>group</v-icon>
          </v-list-tile-action>
          <v-list-tile-content>
            <v-list-tile-title>Groups</v-list-tile-title>
          </v-list-tile-content>
          </v-list-tile>
        </v-list-group>

      </v-list>
    </v-navigation-drawer>

    <v-toolbar color="blue" dark fixed app>
      <v-toolbar-side-icon @click.stop="drawer = !drawer"></v-toolbar-side-icon>
      <v-toolbar-title>BAREOS<span class="subheading"> Open Source Data Protection</span></v-toolbar-title>
      <v-spacer></v-spacer>
      <v-toolbar-items>
        <v-btn flat>Button 1</v-btn>
        <v-btn flat>Button 2</v-btn>
        <v-menu offset-y>
          <v-btn slot="activator" flat>Button 3</v-btn>
          <v-list>
            <v-list-tile @click="">
              <v-list-tile-title>Item 1</v-list-tile-title>
            </v-list-tile>
          </v-list>
        </v-menu>
        <v-btn flat>
          <v-badge color="black">
            <span slot="badge">60</span>
            <v-icon>notifications</v-icon>
          </v-badge>
        </v-btn>
      </v-toolbar-items>
    </v-toolbar>

  </span>
</template>

<script>
export default {
  data: () => ({
    drawer: true,
    items: [
      {
        icon: 'dashboard',
        title: 'Dashboard',
        path: '/dashboard',
        items: []
      },
      {
        icon: 'notifications',
        title: 'Notifications',
        path: '/notifications',
        items: []
      },
      {
        icon: 'archive',
        title: 'Backup',
        path: '/backup',
        items: [
          {
            icon: 'history',
            title: 'History',
            path: '/backup/history'
          },
          {
            icon: 'laptop',
            title: 'Clients',
            path: '/backup/clients'
          },
          {
            icon: 'assignment',
            title: 'Jobs',
            path: '/backup/jobs'
          },
          {
            icon: 'description',
            title: 'Filesets',
            path: '/backup/filesets'
          },
          {
            icon: 'schedule',
            title: 'Schedules',
            path: '/backup/Schedules'
          }
        ]
      },
      {
        icon: 'unarchive',
        title: 'Restore',
        path: '/restore',
        items: []
      },
      {
        icon: 'settings',
        title: 'Settings',
        path: '/settings',
        items: [
          {
            icon: 'person',
            title: 'Users',
            path: '/settings/users'
          },
          {
            icon: 'group',
            title: 'Groups',
            path: '/settings/groups'
          }
        ]
      }
    ]
  }),
  props: {
    source: String
  }
}
</script>
