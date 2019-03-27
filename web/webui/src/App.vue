<template>
  <v-app id="app">
    <MainNavigation />
    <v-content>
      <v-container>
        <router-view></router-view>
      </v-container>
    </v-content>
  </v-app>
</template>

<script>
import MainNavigation from './components/MainNavigation'

export default {
  name: 'BareosWebUI',
  components: {
    MainNavigation,
  },
  beforeCreate() {
    this.$store.commit('initializeStore')
  },
}
</script>

<style>
</style>
