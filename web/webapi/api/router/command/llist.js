const isEmpty = require('lodash/isEmpty')
const difference = require('lodash/difference')
const path = require('path')
const routeBasePath = path.basename(module.filename, '.js')

const bconsoleAsync = require('../../bconsole').bconsoleAsync
const listClients = params => bconsoleAsync(`llist ${params}`)

module.exports = router => {
  router.get(`/${routeBasePath}/jobs`, async (ctx, data) => {
    const validQueryParams = ['job', 'jobstatus']
    const queryKeys = Object.keys(ctx.query)
    const isQueryValid = isEmpty(difference(queryKeys, validQueryParams))

    if (!isQueryValid) {
      ctx.status = 400
    } else {
      let call = 'jobs'
      if (queryKeys.includes('job')) {
        call += ` job=${ctx.query.job}`
      }
      if (queryKeys.includes('jobstatus')) {
        call += ` jobstatus=${ctx.query.jobstatus}`
      }

      const result = await listClients(call)
      ctx.body = result
    }
  })

  router.get(`/${routeBasePath}/job/:jobid`, async (ctx, data) => {
    const jobId = Number(ctx.params.jobid)
    if (!jobId) {
      ctx.status = 400
      ctx.body = 'jobid is not a number'
    } else {
      const result = await listClients(`jobid=${ctx.params.jobid}`)
      ctx.body = result
    }
  })
  router.get(`/${routeBasePath}/joblog/:jobid`, async (ctx, data) => {
    const jobId = Number(ctx.params.jobid)
    if (!jobId) {
      ctx.status = 400
      ctx.body = 'jobid is not a number'
    } else {
      const result = await listClients(`joblog jobid=${ctx.params.jobid}`)
      ctx.body = result
    }
  })

  // backups client=<client-name> [fileset=<fileset-name>] [jobstatus=<status>] [level=<level>] [order=<asc|desc>] [limit=<number>]
  router.get(`/backups/client=:clientName`, async (ctx, data) => {
    const validQueryParams = ['fileset', 'jobstatus', 'level', 'order', 'limit']
    const queryparams = Object.keys(ctx.query)
    const isQueryValid = isEmpty(difference(queryparams, validQueryParams))

    if (!isQueryValid) {
      ctx.status = 400
    } else {
      const result = await listClients(`backups client=${ctx.params.clientName}`)
      ctx.body = result
    }
  })
}

// views
// todo: filter: job, days, status
// llist jobs job=<name> days=[1, 3, 7, 30, 365, all] jobstatus=[all, successful, warning, unsusccessful, runnimg, waiting]

// job details: messages
//  todo: llist joblog jobid=<n>

// used volumes
// todo: llist media jobid=<n>

// basefiles jobid=<jobid> |
// basefiles ujobid=<complete_name> |
// backups client=<client-name> [fileset=<fileset-name>] [jobstatus=<status>] [level=<level>] [order=<asc|desc>] [limit=<number>] |
// clients |
// copies jobid=<jobid> |
// files jobid=<jobid> |
// files ujobid=<complete_name> |
// filesets |
// fileset [ jobid=<jobid> ] |
// fileset [ ujobid=<complete_name> ] |
// fileset [ filesetid=<filesetid> ] |
// fileset [ jobid=<jobid> ] |
// jobs [job=<job-name>] [client=<client-name>] [jobstatus=<status>] [joblevel=<joblevel>] [volume=<volumename>] [days=<number>] [hours=<number>] [last] [count] |
// job=<job-name> [client=<client-name>] [jobstatus=<status>] [volume=<volumename>] [days=<number>] [hours=<number>] |
// jobid=<jobid> |
// ujobid=<complete_name> |
// joblog jobid=<jobid> |
// joblog ujobid=<complete_name> |
// jobmedia jobid=<jobid> |
// jobmedia ujobid=<complete_name> |
// jobtotals |
// jobstatistics jobid=<jobid> |
// log [ limit=<number> [ offset=<number> ] ] [reverse]|
// media [ jobid=<jobid> | ujobid=<complete_name> | pool=<pool-name> | all ] |
// media=<media-name> |
// nextvol job=<job-name> |
// nextvolume ujobid=<complete_name> |
// pools |
// pool=<pool-name> |
// storages |
// volumes [ jobid=<jobid> | ujobid=<complete_name> | pool=<pool-name> | all ] [count] |
// volume=<volume-name> |
// [current] |
// [enabled | disabled] |
// [limit=<number> [offset=<number>]]
