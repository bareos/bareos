<template>
  <b-table
    :data="jobs"
    :bordered="false"
    :striped="true"
    :narrowed="false"
    :hoverable="true"
    :loading="false"
    :focusable="false"
    :mobile-cards="false"
    :selected.sync="selectedJob"
    detailed
    detail-key="jobid"
    :opened-detailed="openedDetails"
    paginated
    per-page="20"
  >
    <template slot-scope="props">
      <b-table-column field="id" label="ID" width="40" numeric>
        {{ props.row.jobid }}
      </b-table-column>
      <b-table-column
        field="jobstatus" label="Status" sortable centered>
        <font-awesome-icon icon="coffee" v-if="props.row.jobstatus === 'T'"/>
        <font-awesome-icon icon="walking" v-if="props.row.jobstatus === 'R'"/>
        <font-awesome-icon icon="exclamation-triangle" v-if="props.row.jobstatus === 'E'"/>
        {{ js(props.row.jobstatus) }}
      </b-table-column>
      <b-table-column field="name" label="Name" sortable>
        {{ props.row.name }}
      </b-table-column>
      <b-table-column field="level" label="Level" sortable centered>
        {{ jl(props.row.level) }}
      </b-table-column>
      <b-table-column field="type" label="Type" sortable centered>
        {{ jt(props.row.type) }}
      </b-table-column>
      <b-table-column field="starttime" label="Started at" sortable centered>
        {{ dateFormat(props.row.starttime) }}
      </b-table-column>
      <b-table-column field="endtime" label="Ended at" sortable centered>
        {{ dateFormat(props.row.endtime) }}
      </b-table-column>
      <!--"job": "backup-bareos-fd.2018-10-01_15.50.49_04",-->
      <!--"purgedfiles": "0",-->
      <!--"level": "F",-->
      <!--"clientid": "1",-->
      <!--"client": "localhost-fd",-->
      <!--"schedtime": "2018-10-01 15:50:38",-->
      <!--"realendtime": "2018-10-01 15:50:52",-->
      <!--"jobtdate": "1538401852",-->
      <!--"volsessionid": "1",-->
      <!--"volsessiontime": "1538387416",-->
      <!--"jobfiles": "201",-->
      <!--"jobbytes": "8103105",-->
      <!--"joberrors": "0",-->
      <!--"jobmissingfiles": "0",-->
      <!--"poolid": "3",-->
      <!--"poolname": "Full",-->
      <!--"priorjobid": "0",-->
      <!--"filesetid": "1",-->
      <!--"fileset": "SelfTest"-->
    </template>
    <template slot="detail" slot-scope="props">
      <article class="media">
        <figure class="media-left">
          <p class="image is-64x64">
            <font-awesome-icon icon="coffee" v-if="props.row.jobstatus === 'T'"/>
            <font-awesome-icon icon="walking" v-if="props.row.jobstatus === 'R'"/>
            <font-awesome-icon icon="exclamation-triangle" v-if="props.row.jobstatus === 'E'"/>
          </p>
        </figure>
        <div class="media-content">
          <div class="content">
            <div class="columns is-multiline">
              <div class="column is-half" v-for="item in getDetails(props.row)" :key="item.key">
                <span class="has-text-weight-bold">{{ item.key }}</span>: <span>{{ item.value }}</span>
              </div>
            </div>
          </div>
        </div>
      </article>
    </template>
    <template slot="empty">
      <section class="section">
        <div class="content has-text-grey has-text-centered">
          <p>
            <font-awesome-icon icon="coffee" size="5x"/>
          </p>
          <p>No jobs</p>
        </div>
      </section>
    </template>
  </b-table>
</template>

<script>
import { getJobs, jobLevels, jobStatus, jobTypes } from '@/models/jobs'
import * as moment from 'moment'

export default {
  name: 'JobListing',
  data () {
    return {
      jobs: [],
      openedDetails: [],
      selectedJob: null
    }
  },
  computed: {},
  watch: {
    selectedJob: function (val) {
      this.$emit('job-selected', Number(val.jobid))
    }
  },
  created: async function () {
    const jobs = await getJobs(this.$http)
    this.jobs = jobs
  },
  methods: {
    js: function (code) {
      return jobStatus.get(code)
    },
    jt: function (code) {
      return jobTypes.get(code)
    },
    jl: function (code) {
      return jobLevels.get(code)
    },
    dateFormat: function (date) {
      return moment(date).fromNow()
    },
    getDetails: function (selectedItem) {
      return Object.entries(selectedItem).map(([key, value]) => ({ key, value }))
    }
  }
}
</script>

<style scoped>
</style>
