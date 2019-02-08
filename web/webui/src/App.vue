<template>
  <div id="app">
    <nav class="navbar is-white" role="navigation" aria-label="main navigation">
      <div class="container">
        <div class="navbar-brand">
          <router-link class="navbar-item" to="/">
            <img src="./assets/bareos-logo-small.png" width="48" height="45">
          </router-link>
        </div>
        <div class="navbar-menu is-active">
          <div class="navbar-start">
            <router-link class="navbar-item" to="/jobs">Jobs</router-link>
          </div>
        </div>
        <div class="navbar-menu is-active">
          <div class="navbar-start">
            <router-link class="navbar-item" to="/bconsole">BConsole</router-link>
          </div>
        </div>
      </div>
    </nav>
    <div class="container">
      <router-view/>
    </div>
  </div>
</template>

<style lang="scss">
  @import "~bulma";
  @import "~buefy/src/scss/buefy";

  html, body {
    font-family: 'Open Sans', serif;
    font-size: 16px;
    line-height: 1.5;
    height: 100%;
    background: #ECF0F3;
  }

  nav.navbar {
    margin-bottom: 1rem;
  }
</style>

<script>
export default {
  name: 'BareosWebUI',
  beforeCreate () {
    this.$store.commit('initializeStore')
  }
}
</script>
