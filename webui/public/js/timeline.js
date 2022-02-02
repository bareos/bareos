/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2022 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

var rsLabel1 = "1 " + iJS._("day");
var rsLabel2 = "3 " + iJS._("days");
var rsLabel3 = "7 " + iJS._("days");
var rsLabel4 = "14 " + iJS._("days");
var rsLabel5 = "31 " + iJS._("days");

$("#rangeslider").bootstrapSlider({
  ticks: [1,3,7,14,31],
  ticks_labels: [rsLabel1, rsLabel2, rsLabel3, rsLabel4, rsLabel5],
  ticks_positions: [0,25,50,75,100],
  step: 1,
  lock_to_ticks: true,
  tooltip: "hide",
  handle: "round",
  value: 1
});

function getSliderValue() {
  let range = document.getElementById("rangeslider");
  return range.value;
}

function updateChartOptions(currenttime, p) {
  let max = currenttime + (60 * 60 * 1000) // current timestamp + 1h
  let min = currenttime - (60 * 60 * 24 * 1000) * p // current timestamp - 24h * p (period)

  chart.updateOptions({
    noData: {
      text: 'Loading ...',
    },
    xaxis: {
      min: min,
      max: max,
    },
    events: {
      beforeZoom: function(ctx) {
        // we need to reset min/max before zoom
        ctx.w.config.xaxis.min = undefined
        ctx.w.config.xaxis.max = undefined
      }
    }
  });
}

function changeNoDataText(d) {
  let msg = "Loading ...";
  if(!d) {
    msg = "No data available";
  }
  chart.updateOptions({
    noData: {
      text: msg
    }
  });
}

