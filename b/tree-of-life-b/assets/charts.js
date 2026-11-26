(function () {
  var D = window.DEMO;
  if (!D || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();
  var bg = style.getPropertyValue('--bg').trim();

  var baseText = {
    fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif',
    color: ink
  };

  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };

  var tooltipCommon = {
    trigger: 'axis',
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 },
    axisPointer: { lineStyle: { color: muted, type: 'dashed' } }
  };

  function shortEnv(name) {
    var map = {
      'Global Warming': 'Warming',
      'Global Cooling': 'Cooling',
      'Low Light Period': 'Low light',
      'Oxygen Rise': 'Oxygen',
      'Toxic Bloom': 'Toxin',
      'Meteor Impact': 'Meteor',
      'Normal Ocean': 'Normal'
    };
    return map[name] || name;
  }

  function envAreas(windows) {
    return windows.map(function (w, i) {
      return [
        {
          xAxis: w.start,
          name: shortEnv(w.name),
          itemStyle: { color: i % 2 === 0 ? 'rgba(184,98,47,0.09)' : 'rgba(184,98,47,0.05)' },
          label: {
            show: true,
            position: 'insideTop',
            color: accent2,
            fontSize: 9,
            fontFamily: 'GeistMono, monospace'
          }
        },
        { xAxis: w.end }
      ];
    });
  }

  function speciationLines(speciations) {
    return speciations.map(function (s) {
      return {
        xAxis: s.t,
        name: s.to,
        lineStyle: { color: accent2, type: 'dashed', width: 1, opacity: 0.75 },
        label: {
          show: true,
          position: 'insideEndTop',
          formatter: '→ ' + s.to,
          color: accent2,
          fontSize: 9,
          fontFamily: 'GeistMono, monospace'
        }
      };
    });
  }

  /* ---------- 图 1 资源产率 ---------- */
  var chartProduction = echarts.init(document.getElementById('chart-production'), null, {
    renderer: 'svg'
  });
  chartProduction.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 58, right: 26, top: 42, bottom: 46 },
    legend: {
      data: ['ATP / s', 'DNA / s'],
      top: 4,
      left: 0,
      itemWidth: 18,
      itemHeight: 2,
      textStyle: { color: muted, fontSize: 11 }
    },
    tooltip: Object.assign({}, tooltipCommon, {
      valueFormatter: function (value) {
        return Number(value).toFixed(2) + ' / s';
      }
    }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 600,
      name: '秒',
      nameGap: 26,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'log',
      logBase: 10,
      name: '每秒产出',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 },
      axisLabel: {
        color: muted,
        fontSize: 11,
        formatter: function (value) {
          if (value >= 1000) {
            return value / 1000 + 'k';
          }
          if (value >= 1) {
            return String(value);
          }
          return value.toFixed(2);
        }
      }
    }),
    series: [
      {
        name: 'ATP / s',
        type: 'line',
        showSymbol: false,
        lineStyle: { color: accent, width: 2 },
        itemStyle: { color: accent },
        areaStyle: { color: 'rgba(30,107,72,0.08)' },
        data: D.era1.map(function (r) {
          return [r.t, r.atpRate];
        }),
        markArea: { silent: true, data: envAreas(D.windows1) },
        markLine: { symbol: 'none', silent: true, data: speciationLines(D.speciations1) }
      },
      {
        name: 'DNA / s',
        type: 'line',
        showSymbol: false,
        lineStyle: { color: accent2, width: 1.6 },
        itemStyle: { color: accent2 },
        data: D.era1.map(function (r) {
          return [r.t, r.dnaRate];
        })
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartProduction.resize();
  });

  /* ---------- 图 2 单轮升级 ---------- */
  var chartUpgrades = echarts.init(document.getElementById('chart-upgrades'), null, {
    renderer: 'svg'
  });
  chartUpgrades.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 48, right: 26, top: 42, bottom: 42 },
    legend: {
      data: ['Metabolism', 'Reproduction', 'Genome'],
      top: 4,
      left: 0,
      itemWidth: 18,
      itemHeight: 2,
      textStyle: { color: muted, fontSize: 11 }
    },
    tooltip: Object.assign({}, tooltipCommon, {
      valueFormatter: function (value) {
        return 'Lv.' + value;
      }
    }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 600,
      name: '秒',
      nameGap: 24,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 12,
      minInterval: 1,
      name: '等级',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        name: 'Metabolism',
        type: 'line',
        step: 'end',
        showSymbol: false,
        lineStyle: { color: accent, width: 2 },
        itemStyle: { color: accent },
        data: D.era1.map(function (r) {
          return [r.t, r.metab];
        })
      },
      {
        name: 'Reproduction',
        type: 'line',
        step: 'end',
        showSymbol: false,
        lineStyle: { color: accent2, width: 1.8 },
        itemStyle: { color: accent2 },
        data: D.era1.map(function (r) {
          return [r.t, r.repro];
        })
      },
      {
        name: 'Genome',
        type: 'line',
        step: 'end',
        showSymbol: false,
        lineStyle: { color: muted, width: 1.6 },
        itemStyle: { color: muted },
        data: D.era1.map(function (r) {
          return [r.t, r.genome];
        })
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartUpgrades.resize();
  });

  /* ---------- 图 3 种群与上限 ---------- */
  var stageBands = [
    { from: 0, to: D.speciations1[0].t, name: 'Protocell' },
    { from: D.speciations1[0].t, to: D.speciations1[1].t, name: 'Cyanobacterium' },
    { from: D.speciations1[1].t, to: D.speciations1[2].t, name: 'Algal Eukaryote' },
    { from: D.speciations1[2].t, to: 600, name: 'Multicellular Colony' }
  ].map(function (band, i) {
    return [
      {
        xAxis: band.from,
        name: band.name,
        itemStyle: { color: i % 2 === 0 ? 'rgba(30,107,72,0.045)' : 'rgba(30,107,72,0.015)' },
        label: {
          show: true,
          position: 'insideTop',
          color: muted,
          fontSize: 10,
          fontFamily: 'GeistMono, monospace'
        }
      },
      { xAxis: band.to }
    ];
  });

  var chartPopulation = echarts.init(document.getElementById('chart-population'), null, {
    renderer: 'svg'
  });
  chartPopulation.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 56, right: 26, top: 42, bottom: 46 },
    legend: {
      data: ['主谱系种群', '种群上限'],
      top: 4,
      left: 0,
      itemWidth: 18,
      itemHeight: 2,
      textStyle: { color: muted, fontSize: 11 }
    },
    tooltip: Object.assign({}, tooltipCommon, {
      valueFormatter: function (value) {
        return Number(value).toFixed(1);
      }
    }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 600,
      name: '秒',
      nameGap: 26,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      name: '种群',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        name: '主谱系种群',
        type: 'line',
        showSymbol: false,
        lineStyle: { color: accent, width: 2 },
        itemStyle: { color: accent },
        areaStyle: { color: 'rgba(30,107,72,0.12)' },
        data: D.era1.map(function (r) {
          return [r.t, r.pop];
        }),
        markArea: { silent: true, data: stageBands }
      },
      {
        name: '种群上限',
        type: 'line',
        step: 'end',
        showSymbol: false,
        lineStyle: { color: accent2, width: 1.4, type: 'dashed' },
        itemStyle: { color: accent2 },
        data: D.era1.map(function (r) {
          return [r.t, r.cap];
        })
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartPopulation.resize();
  });

  /* ---------- 图 4 物种 × 环境适应度矩阵 ---------- */
  var speciesNames = [
    'Protocell',
    'Cyanobacterium',
    'Chemolithotroph',
    'Algal Eukaryote',
    'Protozoan Predator',
    'Multicellular Colony',
    'Extremophile Colony'
  ];
  var eventNames = ['Warming', 'Cooling', 'Low light', 'Oxygen', 'Toxin', 'Meteor'];
  var heatData = D.matrix.map(function (item) {
    return [item.e - 1, item.s, item.m];
  });

  var chartFitness = echarts.init(document.getElementById('chart-fitness'), null, {
    renderer: 'svg'
  });
  chartFitness.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 150, right: 26, top: 24, bottom: 62 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        return (
          speciesNames[params.value[1]] +
          ' × ' +
          eventNames[params.value[0]] +
          '<br/>适应度 ×' +
          Number(params.value[2]).toFixed(2)
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: eventNames,
      splitArea: { show: false },
      axisLabel: { color: ink, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: speciesNames,
      inverse: true,
      splitArea: { show: false },
      axisLabel: { color: ink, fontSize: 11 }
    }),
    visualMap: {
      min: 0.55,
      max: 1.3,
      calculable: false,
      orient: 'horizontal',
      left: 'center',
      bottom: 6,
      itemWidth: 12,
      itemHeight: 90,
      text: ['强适应', '弱适应'],
      textStyle: { color: muted, fontSize: 10 },
      inRange: { color: [bg, accent2, accent] }
    },
    series: [
      {
        type: 'heatmap',
        data: heatData,
        splitArea: { show: false },
        itemStyle: { borderColor: bg2, borderWidth: 2 },
        label: {
          show: true,
          formatter: function (params) {
            return Number(params.value[2]).toFixed(2);
          },
          color: ink,
          fontSize: 10,
          fontFamily: 'GeistMono, monospace'
        },
        emphasis: { itemStyle: { borderColor: ink, borderWidth: 1 } }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartFitness.resize();
  });

  /* ---------- 图 5 突变 DNA 支出 ---------- */
  var mutationNames = D.dnaSpend.map(function (item) {
    return item.name;
  });
  var mutationCosts = D.dnaSpend.map(function (item) {
    return item.cost;
  });
  var mutationTotals = D.dnaSpend.map(function (item) {
    return item.total;
  });

  var chartMutations = echarts.init(document.getElementById('chart-mutations'), null, {
    renderer: 'svg'
  });
  chartMutations.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 56, right: 62, top: 46, bottom: 86 },
    legend: {
      data: ['单次成本', '累计支出'],
      top: 4,
      left: 0,
      itemWidth: 14,
      itemHeight: 8,
      textStyle: { color: muted, fontSize: 11 }
    },
    tooltip: Object.assign({}, tooltipCommon, { trigger: 'axis' }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: mutationNames,
      axisLabel: { color: muted, fontSize: 10, rotate: 32 },
      splitLine: { show: false }
    }),
    yAxis: [
      Object.assign({}, axisCommon, {
        type: 'value',
        name: 'DNA',
        nameGap: 14,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      Object.assign({}, axisCommon, {
        type: 'value',
        name: '累计',
        nameGap: 16,
        nameTextStyle: { color: muted, fontSize: 11 },
        splitLine: { show: false }
      })
    ],
    series: [
      {
        name: '单次成本',
        type: 'bar',
        barWidth: 18,
        itemStyle: { color: accent, opacity: 0.85 },
        data: mutationCosts
      },
      {
        name: '累计支出',
        type: 'line',
        yAxisIndex: 1,
        showSymbol: true,
        symbolSize: 5,
        lineStyle: { color: accent2, width: 1.8 },
        itemStyle: { color: accent2 },
        data: mutationTotals
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartMutations.resize();
  });

  /* ---------- 图 6 EP 瀑布 ---------- */
  var epSteps = [
    { name: '阶段 2E', value: D.ep.stages * 2 },
    { name: '突变 ⌊M/3⌋', value: Math.floor(D.ep.mutations / 3) },
    { name: '物种 ⌊S/2⌋', value: Math.floor(D.ep.species / 2) },
    { name: '危机 min(C,3)', value: Math.min(D.ep.crises, 3) },
    { name: '里程碑 B', value: D.ep.milestone },
    { name: '合计 EP', value: D.ep.ep }
  ];
  var epBase = [];
  var epValue = [];
  var running = 0;
  epSteps.forEach(function (step, index) {
    if (index === epSteps.length - 1) {
      epBase.push(0);
      epValue.push(step.value);
    } else {
      epBase.push(running);
      epValue.push(step.value);
      running += step.value;
    }
  });

  var chartEp = echarts.init(document.getElementById('chart-ep'), null, { renderer: 'svg' });
  chartEp.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 108, right: 40, top: 24, bottom: 34 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      trigger: 'axis',
      axisPointer: { type: 'shadow' }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 20,
      minInterval: 1
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'category',
      inverse: true,
      data: epSteps.map(function (step) {
        return step.name;
      }),
      axisLabel: { color: ink, fontSize: 11 },
      splitLine: { show: false }
    }),
    series: [
      {
        name: 'base',
        type: 'bar',
        stack: 'ep',
        silent: true,
        itemStyle: { color: 'transparent' },
        emphasis: { itemStyle: { color: 'transparent' } },
        data: epBase
      },
      {
        name: 'EP',
        type: 'bar',
        stack: 'ep',
        barWidth: 18,
        itemStyle: {
          color: function (params) {
            return params.dataIndex === epSteps.length - 1 ? accent2 : accent;
          },
          borderRadius: [0, 2, 2, 0]
        },
        label: {
          show: true,
          position: 'right',
          formatter: '{c} EP',
          color: ink,
          fontSize: 11,
          fontFamily: 'GeistMono, monospace'
        },
        data: epValue
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartEp.resize();
  });

  /* ---------- 图 7 永久强化 ---------- */
  var branchNames = ['Metabolism', 'Mutation', 'Adaptation', 'Diversification'];
  var levels = [0, 0, 0, 0];
  D.legacy.forEach(function (item) {
    levels[item.branch] = Math.max(levels[item.branch], item.level);
  });

  var chartLegacy = echarts.init(document.getElementById('chart-legacy'), null, {
    renderer: 'svg'
  });
  chartLegacy.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 98, right: 46, top: 18, bottom: 30 },
    tooltip: Object.assign({}, tooltipCommon, { axisPointer: { type: 'none' } }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 5,
      minInterval: 1
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'category',
      inverse: true,
      data: branchNames,
      axisLabel: { color: ink, fontSize: 12 },
      splitLine: { show: false }
    }),
    series: [
      {
        type: 'bar',
        barWidth: 18,
        showBackground: true,
        backgroundStyle: { color: 'rgba(30,107,72,0.07)' },
        itemStyle: { color: accent, borderRadius: [0, 2, 2, 0] },
        label: {
          show: true,
          position: 'right',
          formatter: function (params) {
            return 'Lv.' + params.value + ' / 5';
          },
          color: ink,
          fontSize: 11,
          fontFamily: 'GeistMono, monospace'
        },
        data: levels
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartLegacy.resize();
  });

  /* ---------- 图 8 两轮对比 ---------- */
  var rate1 = D.rate60.era1;
  var rate2 = D.rate60.era2;

  var chartCompare = echarts.init(document.getElementById('chart-compare'), null, {
    renderer: 'svg'
  });
  chartCompare.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 62, right: 26, top: 42, bottom: 34 },
    legend: {
      data: ['第一轮', '第二轮'],
      top: 4,
      left: 0,
      itemWidth: 14,
      itemHeight: 8,
      textStyle: { color: muted, fontSize: 11 }
    },
    tooltip: Object.assign({}, tooltipCommon, {
      valueFormatter: function (value) {
        return Number(value).toFixed(2) + ' / s';
      }
    }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: ['ATP / s', 'Biomass / s'],
      axisLabel: { color: ink, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      name: '第 60 秒产率',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        name: '第一轮',
        type: 'bar',
        barWidth: 26,
        itemStyle: { color: 'rgba(93,107,99,0.45)' },
        data: [rate1.atpRate, rate1.bioRate],
        label: {
          show: true,
          position: 'top',
          formatter: function (params) {
            return params.value.toFixed(2);
          },
          color: muted,
          fontSize: 10,
          fontFamily: 'GeistMono, monospace'
        }
      },
      {
        name: '第二轮',
        type: 'bar',
        barWidth: 26,
        itemStyle: { color: accent },
        data: [rate2.atpRate, rate2.bioRate],
        label: {
          show: true,
          position: 'top',
          formatter: function (params) {
            return params.value.toFixed(2);
          },
          color: accent,
          fontSize: 10,
          fontFamily: 'GeistMono, monospace'
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartCompare.resize();
  });

  /* ---------- 图 9 图鉴进度 ---------- */
  var chartCodex = echarts.init(document.getElementById('chart-codex'), null, {
    renderer: 'svg'
  });
  chartCodex.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 92, right: 96, top: 18, bottom: 30 },
    tooltip: Object.assign({}, tooltipCommon, { axisPointer: { type: 'none' } }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 100,
      axisLabel: { color: muted, fontSize: 10, formatter: '{value}%' }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'category',
      inverse: true,
      data: ['突变图鉴', '物种图鉴'],
      axisLabel: { color: ink, fontSize: 12 },
      splitLine: { show: false }
    }),
    series: [
      {
        type: 'bar',
        barWidth: 18,
        showBackground: true,
        backgroundStyle: { color: 'rgba(30,107,72,0.07)' },
        itemStyle: { color: accent, borderRadius: [0, 2, 2, 0] },
        label: {
          show: true,
          position: 'right',
          formatter: function (params) {
            return params.data.label;
          },
          color: ink,
          fontSize: 11,
          fontFamily: 'GeistMono, monospace'
        },
        data: [
          {
            value: (D.codex.mutations / D.codex.mutationsTotal) * 100,
            label: D.codex.mutations + ' / ' + D.codex.mutationsTotal
          },
          {
            value: (D.codex.species / D.codex.speciesTotal) * 100,
            label: D.codex.species + ' / ' + D.codex.speciesTotal
          }
        ]
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartCodex.resize();
  });

  /* ---------- 图 11 批量种子 EP ---------- */
  var batch = D.batch;
  var avgEp = batch.reduce(function (sum, item) {
    return sum + item.ep;
  }, 0) / batch.length;

  var chartSeedsEp = echarts.init(document.getElementById('chart-seeds-ep'), null, {
    renderer: 'svg'
  });
  chartSeedsEp.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 52, right: 70, top: 34, bottom: 40 },
    legend: {
      data: ['光合路线', '化能路线'],
      top: 2,
      left: 0,
      itemWidth: 12,
      itemHeight: 8,
      textStyle: { color: muted, fontSize: 11 }
    },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        var item = batch[params.dataIndex];
        return (
          '运行 #' +
          item.i +
          ' · seed ' +
          item.seed +
          '<br/>EP ' +
          item.ep +
          ' · 首次分化 ' +
          item.firstSpe.toFixed(1) +
          's<br/>危机 ' +
          item.crises +
          ' · 最高 Biomass ' +
          Math.round(item.maxBiomass)
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: batch.map(function (item) {
        return '#' + item.i;
      }),
      axisLabel: { color: muted, fontSize: 9, interval: 1 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 10,
      max: 20,
      name: 'EP',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        name: '光合路线',
        type: 'bar',
        barWidth: '55%',
        data: batch.map(function (item) {
          return item.firstSpecies === 1 ? item.ep : '-';
        }),
        itemStyle: { color: accent }
      },
      {
        name: '化能路线',
        type: 'bar',
        barWidth: '55%',
        data: batch.map(function (item) {
          return item.firstSpecies === 2 ? item.ep : '-';
        }),
        itemStyle: { color: accent2 },
        markLine: {
          symbol: 'none',
          silent: true,
          lineStyle: { color: ink, type: 'dashed', width: 1 },
          label: {
            formatter: '均值 ' + avgEp.toFixed(2),
            color: ink,
            fontSize: 10,
            position: 'insideEndTop'
          },
          data: [{ yAxis: avgEp }]
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartSeedsEp.resize();
  });

  /* ---------- 图 12 分化时点与 EP ---------- */
  function seedPoints(route) {
    return batch
      .filter(function (item) {
        return item.firstSpecies === route;
      })
      .map(function (item) {
        return [item.firstSpe, item.ep, item.i];
      });
  }

  var chartSeedsTime = echarts.init(document.getElementById('chart-seeds-time'), null, {
    renderer: 'svg'
  });
  chartSeedsTime.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 52, right: 26, top: 34, bottom: 46 },
    legend: {
      data: ['光合路线', '化能路线'],
      top: 2,
      left: 0,
      itemWidth: 10,
      itemHeight: 10,
      textStyle: { color: muted, fontSize: 11 }
    },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        return (
          '运行 #' +
          params.value[2] +
          '<br/>首次分化 ' +
          params.value[0].toFixed(1) +
          's<br/>EP ' +
          params.value[1]
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 180,
      max: 300,
      name: '首次分化（秒）',
      nameGap: 26,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 10,
      max: 20,
      name: 'EP',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        name: '光合路线',
        type: 'scatter',
        symbolSize: 9,
        itemStyle: { color: accent, opacity: 0.85 },
        data: seedPoints(1)
      },
      {
        name: '化能路线',
        type: 'scatter',
        symbolSize: 9,
        itemStyle: { color: accent2, opacity: 0.85 },
        data: seedPoints(2)
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartSeedsTime.resize();
  });

  /* ---------- 图 13 突变获取时点分布 ---------- */
  var timeCategories = D.mutationTimes.map(function (item) {
    return D.mutationNames[item.id];
  });
  var timePoints = [];
  D.mutationTimes.forEach(function (item, categoryIndex) {
    item.times.forEach(function (value) {
      timePoints.push([categoryIndex, value]);
    });
  });

  var chartSeedsMut = echarts.init(document.getElementById('chart-seeds-mut'), null, {
    renderer: 'svg'
  });
  chartSeedsMut.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 52, right: 26, top: 24, bottom: 96 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        return (
          D.mutationNames[D.mutationTimes[params.value[0]].id] +
          '<br/>获取时间 ' +
          params.value[1].toFixed(1) +
          's'
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: timeCategories,
      axisLabel: { color: muted, fontSize: 9, rotate: 34 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      name: '获取时间（秒）',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        type: 'scatter',
        symbolSize: 6,
        itemStyle: { color: accent, opacity: 0.55 },
        data: timePoints,
        emphasis: { itemStyle: { color: accent2, opacity: 1 } }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartSeedsMut.resize();
  });

  /* ---------- 图 14 危机通过余量 ---------- */
  var crisisLabels = D.crisis.map(function (item, index) {
    return shortEnv(
      ['', 'Warming', 'Cooling', 'Low light', 'Oxygen', 'Toxin', 'Meteor'][item.env] || ''
    ) + ' #' + (index + 1);
  });
  var crisisMargin = D.crisis.map(function (item) {
    return Number((item.margin * 100).toFixed(0));
  });

  var chartCrisis = echarts.init(document.getElementById('chart-crisis'), null, {
    renderer: 'svg'
  });
  chartCrisis.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 62, right: 30, top: 34, bottom: 34 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        var item = D.crisis[params.dataIndex];
        return (
          '开始种群 ' +
          item.start.toFixed(1) +
          ' → 结束 ' +
          item.end.toFixed(1) +
          '<br/>通过线 ' +
          item.threshold.toFixed(1) +
          ' · 余量 ' +
          (item.margin * 100).toFixed(0) +
          '%'
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: crisisLabels,
      axisLabel: { color: ink, fontSize: 10 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      name: '相对通过线（%）',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        type: 'bar',
        barWidth: 34,
        itemStyle: { color: accent, borderRadius: [2, 2, 0, 0] },
        label: {
          show: true,
          position: 'top',
          formatter: '{c}%',
          color: accent,
          fontSize: 11,
          fontFamily: 'GeistMono, monospace'
        },
        data: crisisMargin,
        markLine: {
          symbol: 'none',
          silent: true,
          lineStyle: { color: accent2, type: 'dashed', width: 1 },
          label: {
            formatter: '通过线 100%',
            color: accent2,
            fontSize: 10,
            position: 'insideEndTop'
          },
          data: [{ yAxis: 100 }]
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartCrisis.resize();
  });

  /* ---------- 图 15 突变成本与收益 ---------- */
  var impactPoints = D.mutImpact.map(function (item) {
    var total = item.atp + item.bio + item.dna + item.cap + item.growth;
    return {
      value: [item.cost, Number(total.toFixed(1))],
      name: D.mutationNames[item.id],
      symbolSize: 12 + (item.cap + item.growth) / 4,
      itemStyle: { color: item.cost >= 50 ? accent2 : accent, opacity: 0.85 }
    };
  });

  var chartImpact = echarts.init(document.getElementById('chart-mut-impact'), null, {
    renderer: 'svg'
  });
  chartImpact.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 58, right: 96, top: 26, bottom: 44 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        return params.name + '<br/>DNA ' + params.value[0] + ' · 总收益 ' + params.value[1] + '%';
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      name: 'DNA 成本',
      nameGap: 26,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      name: '总收益（%）',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        type: 'scatter',
        data: impactPoints,
        label: {
          show: true,
          position: 'right',
          formatter: '{b}',
          color: muted,
          fontSize: 9
        },
        emphasis: { label: { color: ink, fontWeight: 'bold' } }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartImpact.resize();
  });

  /* ---------- 图 9 突变池可用性 ---------- */
  var poolSpecies = [
    'Protocell', 'Cyano', 'Chemo', 'Algal', 'Protozoan', 'Multi', 'Extremo'
  ];
  var poolData = D.avail.map(function (item) {
    return {
      value: [item.s, item.m, item.a],
      itemStyle: { color: item.a ? accent : 'rgba(221,215,201,0.55)' }
    };
  });

  var chartPool = echarts.init(document.getElementById('chart-pool'), null, {
    renderer: 'svg'
  });
  chartPool.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 152, right: 26, top: 24, bottom: 66 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        return (
          D.mutationNames[params.value[1]] +
          '<br/>' +
          poolSpecies[params.value[0]] +
          ' · ' +
          (params.value[2] ? '可用' : '不可用')
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: poolSpecies,
      splitArea: { show: false },
      axisLabel: { color: ink, fontSize: 10, rotate: 26 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: D.mutationNames,
      inverse: true,
      splitArea: { show: false },
      axisLabel: { color: ink, fontSize: 10 }
    }),
    series: [
      {
        type: 'heatmap',
        data: poolData,
        splitArea: { show: false },
        itemStyle: { borderColor: bg2, borderWidth: 2 },
        label: {
          show: true,
          formatter: function (params) {
            return params.value[2] ? '✓' : '';
          },
          color: bg2,
          fontSize: 11,
          fontFamily: 'GeistMono, monospace'
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartPool.resize();
  });

  /* ---------- 图 10 候选出现概率 ---------- */
  var candidateLevels = D.candidates;
  var candidateSeries = candidateLevels.map(function (scenario, index) {
    var colors = [muted, accent, accent2, ink];
    return {
      name: 'Mutation Lv.' + scenario.level,
      type: 'bar',
      barWidth: 9,
      itemStyle: { color: colors[index], opacity: index === 0 ? 0.55 : 0.9 },
      data: scenario.counts.map(function (count) {
        return Number(((count / 2000) * 100).toFixed(1));
      })
    };
  });

  var chartCandidates = echarts.init(document.getElementById('chart-candidates'), null, {
    renderer: 'svg'
  });
  chartCandidates.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 58, right: 26, top: 40, bottom: 96 },
    legend: {
      data: candidateLevels.map(function (scenario) {
        return 'Mutation Lv.' + scenario.level;
      }),
      top: 2,
      left: 0,
      itemWidth: 10,
      itemHeight: 8,
      textStyle: { color: muted, fontSize: 10 }
    },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      trigger: 'axis',
      formatter: function (params) {
        var lines = [params[0].axisValue];
        params.forEach(function (item) {
          lines.push(item.marker + item.seriesName + ' · ' + item.value + '%');
        });
        return lines.join('<br/>');
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: D.mutationNames,
      axisLabel: { color: muted, fontSize: 9, rotate: 34 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      name: '每次开候选出现概率（%）',
      nameGap: 16,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: candidateSeries
  });
  window.addEventListener('resize', function () {
    chartCandidates.resize();
  });

  /* ---------- 图 11 突变的五种环境特征 ---------- */
  var traitNames = ['Heat', 'Cold', 'Low light', 'Oxygen', 'Toxin'];
  var traitData = [];
  D.mtraits.forEach(function (item) {
    item.v.forEach(function (value, traitIndex) {
      traitData.push([traitIndex, item.id, value]);
    });
  });

  var chartTraits = echarts.init(document.getElementById('chart-traits'), null, {
    renderer: 'svg'
  });
  chartTraits.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 152, right: 26, top: 24, bottom: 62 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        return (
          D.mutationNames[params.value[1]] +
          ' × ' +
          traitNames[params.value[0]] +
          '：' +
          params.value[2]
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: traitNames,
      splitArea: { show: false },
      axisLabel: { color: ink, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: D.mutationNames,
      inverse: true,
      splitArea: { show: false },
      axisLabel: { color: ink, fontSize: 10 }
    }),
    visualMap: {
      min: -2,
      max: 2,
      calculable: false,
      orient: 'horizontal',
      left: 'center',
      bottom: 2,
      itemWidth: 12,
      itemHeight: 70,
      text: ['有利', '不利'],
      textStyle: { color: muted, fontSize: 10 },
      inRange: { color: [accent2, bg2, accent] }
    },
    series: [
      {
        type: 'heatmap',
        data: traitData,
        splitArea: { show: false },
        itemStyle: { borderColor: bg2, borderWidth: 2 },
        label: {
          show: true,
          formatter: function (params) {
            var value = params.value[2];
            return value > 0 ? '+' + value : String(value);
          },
          color: ink,
          fontSize: 10,
          fontFamily: 'GeistMono, monospace'
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartTraits.resize();
  });

  /* ---------- 图 13 图鉴发现曲线 ---------- */
  var chartDiscovery = echarts.init(document.getElementById('chart-discovery'), null, {
    renderer: 'svg'
  });
  chartDiscovery.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 52, right: 26, top: 40, bottom: 42 },
    legend: {
      data: ['已发现突变', '已发现物种'],
      top: 2,
      left: 0,
      itemWidth: 18,
      itemHeight: 2,
      textStyle: { color: muted, fontSize: 11 }
    },
    tooltip: Object.assign({}, tooltipCommon, {
      valueFormatter: function (value) {
        return value + ' 个';
      }
    }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 600,
      name: '秒',
      nameGap: 24,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 12,
      minInterval: 1,
      name: '数量',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        name: '已发现突变',
        type: 'line',
        step: 'end',
        showSymbol: false,
        lineStyle: { color: accent, width: 2 },
        areaStyle: { color: 'rgba(30,107,72,0.08)' },
        data: D.era1.map(function (r) {
          return [r.t, r.discMut];
        })
      },
      {
        name: '已发现物种',
        type: 'line',
        step: 'end',
        showSymbol: false,
        lineStyle: { color: accent2, width: 2 },
        data: D.era1.map(function (r) {
          return [r.t, r.discSpe];
        })
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartDiscovery.resize();
  });

  /* ---------- 图 14/15 双物种槽实验 ---------- */
  var chartDualPop = echarts.init(document.getElementById('chart-dual-pop'), null, {
    renderer: 'svg'
  });
  chartDualPop.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 54, right: 26, top: 40, bottom: 42 },
    legend: {
      data: ['主谱系 · Cyanobacterium', '额外槽 · Protocell'],
      top: 2,
      left: 0,
      itemWidth: 18,
      itemHeight: 2,
      textStyle: { color: muted, fontSize: 10 }
    },
    tooltip: Object.assign({}, tooltipCommon, {
      valueFormatter: function (value) {
        return Number(value).toFixed(1);
      }
    }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 180,
      name: '秒',
      nameGap: 24,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      name: '种群',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        name: '主谱系 · Cyanobacterium',
        type: 'line',
        showSymbol: false,
        lineStyle: { color: accent, width: 2 },
        areaStyle: { color: 'rgba(30,107,72,0.1)' },
        data: D.dual.map(function (r) {
          return [r.t, r.popMain];
        })
      },
      {
        name: '额外槽 · Protocell',
        type: 'line',
        showSymbol: false,
        lineStyle: { color: accent2, width: 2 },
        data: D.dual.map(function (r) {
          return [r.t, r.popExtra];
        })
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartDualPop.resize();
  });

  var chartDualAtp = echarts.init(document.getElementById('chart-dual-atp'), null, {
    renderer: 'svg'
  });
  chartDualAtp.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 54, right: 26, top: 40, bottom: 42 },
    legend: {
      data: ['主谱系', '额外槽', '合计'],
      top: 2,
      left: 0,
      itemWidth: 18,
      itemHeight: 2,
      textStyle: { color: muted, fontSize: 10 }
    },
    tooltip: Object.assign({}, tooltipCommon, {
      valueFormatter: function (value) {
        return Number(value).toFixed(2) + ' / s';
      }
    }),
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      max: 180,
      name: '秒',
      nameGap: 24,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 0,
      name: 'ATP / s',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        name: '主谱系',
        type: 'line',
        showSymbol: false,
        lineStyle: { color: accent, width: 2 },
        data: D.dual.map(function (r) {
          return [r.t, r.atpMain];
        })
      },
      {
        name: '额外槽',
        type: 'line',
        showSymbol: false,
        lineStyle: { color: accent2, width: 2 },
        data: D.dual.map(function (r) {
          return [r.t, r.atpExtra];
        })
      },
      {
        name: '合计',
        type: 'line',
        showSymbol: false,
        lineStyle: { color: ink, width: 1.4, type: 'dashed' },
        data: D.dual.map(function (r) {
          return [r.t, r.atpTotal];
        })
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartDualAtp.resize();
  });

  /* ---------- 结构图 ---------- */
  if (window.mermaid) {
    window.mermaid.initialize({ startOnLoad: false, theme: 'neutral', securityLevel: 'loose' });
    window.mermaid.run({ querySelector: '.mermaid' });
  }
})();

(function () {
  var S = window.SENS;
  if (!S || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var labels = S.scenarios.map(function (item) {
    return item.label;
  });

  function boxColors(index) {
    return index === 0
      ? { color: accent2, borderColor: accent2 }
      : { color: accent, borderColor: accent, opacity: 0.8 };
  }

  function boxData(field) {
    return S.scenarios.map(function (item) {
      return item[field].box;
    });
  }

  var chartSensEp = echarts.init(document.getElementById('chart-sens-ep'), null, {
    renderer: 'svg'
  });
  chartSensEp.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 50, right: 26, top: 26, bottom: 42 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        var scenario = S.scenarios[params.dataIndex];
        var box = scenario.ep.box;
        return (
          scenario.label +
          '<br/>EP 均值 ' +
          scenario.ep.mean +
          '<br/>最小 ' +
          box[0] +
          ' · Q1 ' +
          box[1] +
          ' · 中位 ' +
          box[2] +
          ' · Q3 ' +
          box[3] +
          ' · 最大 ' +
          box[4]
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: labels,
      axisLabel: { color: ink, fontSize: 10, interval: 0, rotate: 16 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 15,
      max: 19,
      name: 'EP',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        type: 'boxplot',
        boxWidth: [14, 34],
        data: boxData('ep').map(function (box, index) {
          return { value: box, itemStyle: boxColors(index) };
        }),
        markLine: {
          symbol: 'none',
          silent: true,
          lineStyle: { color: muted, type: 'dashed', width: 1 },
          label: {
            formatter: '基准均值 ' + S.scenarios[0].ep.mean,
            color: muted,
            fontSize: 10,
            position: 'insideEndTop'
          },
          data: [{ yAxis: S.scenarios[0].ep.mean }]
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartSensEp.resize();
  });

  var chartSensSpe = echarts.init(document.getElementById('chart-sens-spe'), null, {
    renderer: 'svg'
  });
  chartSensSpe.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 56, right: 26, top: 26, bottom: 42 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        var scenario = S.scenarios[params.dataIndex];
        var box = scenario.speciation.box;
        return (
          scenario.label +
          '<br/>首分化均值 ' +
          scenario.speciation.mean +
          's<br/>最小 ' +
          box[0] +
          ' · Q1 ' +
          box[1] +
          ' · 中位 ' +
          box[2] +
          ' · Q3 ' +
          box[3] +
          ' · 最大 ' +
          box[4]
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: labels,
      axisLabel: { color: ink, fontSize: 10, interval: 0, rotate: 16 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 180,
      max: 300,
      name: '首次分化（秒）',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        type: 'boxplot',
        boxWidth: [14, 34],
        data: boxData('speciation').map(function (box, index) {
          return { value: box, itemStyle: boxColors(index) };
        }),
        markLine: {
          symbol: 'none',
          silent: true,
          lineStyle: { color: muted, type: 'dashed', width: 1 },
          label: {
            formatter: '基准均值 ' + S.scenarios[0].speciation.mean + 's',
            color: muted,
            fontSize: 10,
            position: 'insideEndTop'
          },
          data: [{ yAxis: S.scenarios[0].speciation.mean }]
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartSensSpe.resize();
  });

  var tradeoff = S.scenarios.map(function (item) {
    return {
      name: item.label,
      value: [item.speciation.mean, item.ep.mean],
      itemStyle: { color: item.key === 'base' ? accent2 : accent, opacity: 0.9 }
    };
  });

  var chartSensTrade = echarts.init(document.getElementById('chart-sens-tradeoff'), null, {
    renderer: 'svg'
  });
  chartSensTrade.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 56, right: 96, top: 26, bottom: 44 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        return (
          params.name +
          '<br/>首分化 ' +
          params.value[0] +
          's<br/>平均 EP ' +
          params.value[1]
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 195,
      max: 250,
      name: '首次分化（秒）',
      nameGap: 26,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 16.5,
      max: 18.5,
      name: '平均 EP',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        type: 'scatter',
        symbolSize: 13,
        data: tradeoff,
        label: {
          show: true,
          position: 'right',
          formatter: '{b}',
          color: muted,
          fontSize: 10
        },
        emphasis: { label: { color: ink, fontWeight: 'bold' } }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartSensTrade.resize();
  });

  var chartSensBio = echarts.init(document.getElementById('chart-sens-bio'), null, {
    renderer: 'svg'
  });
  chartSensBio.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 62, right: 26, top: 26, bottom: 42 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        return params.name + '<br/>最高 Biomass 均值 ' + params.value;
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: labels,
      axisLabel: { color: ink, fontSize: 10, interval: 0, rotate: 16 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      name: 'Biomass',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 },
      axisLabel: {
        color: muted,
        fontSize: 10,
        formatter: function (value) {
          return value >= 1000 ? value / 1000 + 'k' : value;
        }
      }
    }),
    series: [
      {
        type: 'bar',
        barWidth: 34,
        data: S.scenarios.map(function (item, index) {
          return {
            value: item.biomass.mean,
            itemStyle: { color: index === 0 ? accent2 : accent, opacity: 0.85 }
          };
        }),
        label: {
          show: true,
          position: 'top',
          formatter: function (params) {
            return (params.value / 1000).toFixed(1) + 'k';
          },
          color: ink,
          fontSize: 10,
          fontFamily: 'GeistMono, monospace'
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartSensBio.resize();
  });
})();

(function () {
  var O = window.OPT;
  if (!O || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var labels = [O.before.label, O.after.label];

  var chartOptEp = echarts.init(document.getElementById('chart-opt-ep'), null, {
    renderer: 'svg'
  });
  chartOptEp.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 50, right: 26, top: 26, bottom: 38 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        var item = params.dataIndex === 0 ? O.before : O.after;
        return (
          item.label +
          '<br/>平均 EP ' +
          item.ep.mean +
          '<br/>范围 ' +
          item.ep.min +
          ' – ' +
          item.ep.max
        );
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: labels,
      axisLabel: { color: ink, fontSize: 11 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 16,
      max: 19,
      name: 'EP',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        type: 'boxplot',
        boxWidth: [22, 56],
        data: [
          { value: O.before.ep.box, itemStyle: { color: accent2, borderColor: accent2 } },
          { value: O.after.ep.box, itemStyle: { color: accent, borderColor: accent } }
        ],
        label: {
          show: true,
          position: 'top',
          formatter: function (params) {
            return params.dataIndex === 0 ? O.before.ep.mean : O.after.ep.mean;
          },
          color: ink,
          fontSize: 10,
          fontFamily: 'GeistMono, monospace'
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartOptEp.resize();
  });

  var chartOptSpe = echarts.init(document.getElementById('chart-opt-spe'), null, {
    renderer: 'svg'
  });
  chartOptSpe.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 56, right: 26, top: 26, bottom: 38 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        var item = params.dataIndex === 0 ? O.before : O.after;
        return item.label + '<br/>首次分化均值 ' + item.speciation.mean + 's';
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: labels,
      axisLabel: { color: ink, fontSize: 11 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      min: 200,
      max: 280,
      name: '首次分化（秒）',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 }
    }),
    series: [
      {
        type: 'boxplot',
        boxWidth: [22, 56],
        data: [
          { value: O.before.speciation.box, itemStyle: { color: accent2, borderColor: accent2 } },
          { value: O.after.speciation.box, itemStyle: { color: accent, borderColor: accent } }
        ],
        label: {
          show: true,
          position: 'top',
          formatter: function (params) {
            return (params.dataIndex === 0 ? O.before.speciation.mean : O.after.speciation.mean) + 's';
          },
          color: ink,
          fontSize: 10,
          fontFamily: 'GeistMono, monospace'
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartOptSpe.resize();
  });

  var chartOptBio = echarts.init(document.getElementById('chart-opt-bio'), null, {
    renderer: 'svg'
  });
  chartOptBio.setOption({
    animation: false,
    textStyle: baseText,
    grid: { left: 62, right: 26, top: 26, bottom: 38 },
    tooltip: {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 },
      formatter: function (params) {
        return params.name + '<br/>平均最高 Biomass ' + params.value;
      }
    },
    xAxis: Object.assign({}, axisCommon, {
      type: 'category',
      data: labels,
      axisLabel: { color: ink, fontSize: 11 },
      splitLine: { show: false }
    }),
    yAxis: Object.assign({}, axisCommon, {
      type: 'value',
      name: 'Biomass',
      nameGap: 14,
      nameTextStyle: { color: muted, fontSize: 11 },
      axisLabel: {
        color: muted,
        fontSize: 10,
        formatter: function (value) {
          return value >= 1000 ? value / 1000 + 'k' : value;
        }
      }
    }),
    series: [
      {
        type: 'bar',
        barWidth: 56,
        data: [
          { value: O.before.biomass.mean, itemStyle: { color: accent2, opacity: 0.85 } },
          { value: O.after.biomass.mean, itemStyle: { color: accent, opacity: 0.9 } }
        ],
        label: {
          show: true,
          position: 'top',
          formatter: function (params) {
            return (params.value / 1000).toFixed(1) + 'k';
          },
          color: ink,
          fontSize: 11,
          fontFamily: 'GeistMono, monospace'
        }
      }
    ]
  });
  window.addEventListener('resize', function () {
    chartOptBio.resize();
  });
})();

/* ---------------- 可玩性扩展：突变共振 / 物种基石 / 长期成长 ---------------- */
(function () {
  var E = window.EXP;
  var D = window.DEMO;
  if (!E || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var palette = [accent, accent2, '#c9a227', '#6b8fbf', '#8a6bbf'];
  var statNames = ['ATP', 'Biomass', 'DNA', '上限', '增长'];

  var synergyEl = document.getElementById('chart-synergy');
  if (synergyEl) {
    var synergyChart = echarts.init(synergyEl, null, { renderer: 'svg' });
    synergyChart.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 116, right: 34, top: 34, bottom: 34 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: {
        appendToBody: true,
        backgroundColor: bg2,
        borderColor: rule,
        textStyle: { color: ink, fontSize: 12 },
        formatter: function (params) {
          var s = E.synergies[params[0].dataIndex];
          var lines = [];
          ['atp', 'bio', 'dna', 'cap', 'growth'].forEach(function (key, i) {
            if (s[key] > 0) {
              lines.push(statNames[i] + ' +' + s[key].toFixed(0) + '%');
            }
          });
          var parts = D && D.mutationNames ? D.mutationNames[s.a] + ' + ' + D.mutationNames[s.b] : '';
          return '<b>' + s.name + '</b><br/>' + parts + '<br/>' + lines.join('<br/>');
        }
      },
      xAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: '加成 %',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: E.synergies.map(function (s) { return s.name; }),
        axisLabel: { color: ink, fontSize: 11 },
        splitLine: { show: false }
      }),
      series: ['atp', 'bio', 'dna', 'cap', 'growth'].map(function (key, i) {
        return {
          name: statNames[i],
          type: 'bar',
          barMaxWidth: 14,
          itemStyle: { color: palette[i], opacity: 0.9 },
          data: E.synergies.map(function (s) { return s[key]; })
        };
      })
    });
    window.addEventListener('resize', function () { synergyChart.resize(); });
  }

  var keyEl = document.getElementById('chart-keystone');
  if (keyEl) {
    var keyChart = echarts.init(keyEl, null, { renderer: 'svg' });
    var keyStats = [
      ['ATP', 'atp'], ['Biomass', 'bio'], ['DNA', 'dna'], ['上限', 'cap'],
      ['增长', 'growth'], ['低光', 'lowlight'], ['毒素', 'toxin'], ['减伤', 'crisis']
    ];
    var keyData = [];
    E.keystones.forEach(function (k) {
      keyStats.forEach(function (stat, si) {
        keyData.push([si, k.id, k[stat[1]]]);
      });
    });
    keyChart.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 76, right: 24, top: 24, bottom: 58 },
      tooltip: {
        appendToBody: true,
        backgroundColor: bg2,
        borderColor: rule,
        textStyle: { color: ink, fontSize: 12 },
        formatter: function (params) {
          var k = E.keystones[params.value[1]];
          return '<b>' + k.name + '</b><br/>' + keyStats[params.value[0]][0] + ' +' + params.value[2].toFixed(0) + '%';
        }
      },
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: keyStats.map(function (s) { return s[0]; }),
        axisLabel: { color: ink, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'category',
        inverse: true,
        data: ['Protocell', 'Cyano', 'Chemo', 'Algal', 'Protozoan', 'Multi', 'Extremo'],
        axisLabel: { color: ink, fontSize: 11 },
        splitLine: { show: false }
      }),
      visualMap: {
        min: 0,
        max: 15,
        calculable: false,
        orient: 'horizontal',
        left: 'center',
        bottom: 0,
        itemWidth: 12,
        itemHeight: 90,
        textStyle: { color: muted, fontSize: 10 },
        inRange: { color: ['#f3efe6', accent, accent2] }
      },
      series: [{
        type: 'heatmap',
        data: keyData,
        label: {
          show: true,
          fontSize: 10,
          formatter: function (params) {
            return params.value[2] > 0 ? '+' + params.value[2].toFixed(0) : '';
          }
        },
        itemStyle: { borderColor: '#fff', borderWidth: 1 }
      }]
    });
    window.addEventListener('resize', function () { keyChart.resize(); });
  }

  var metaEl = document.getElementById('chart-meta');
  if (metaEl) {
    var metaChart = echarts.init(metaEl, null, { renderer: 'svg' });
    var maxN = E.codexM[E.codexM.length - 1].n;
    var xs = [];
    for (var n = 0; n <= maxN; ++n) { xs.push(n); }
    function seriesFrom(items, key) {
      var map = {};
      items.forEach(function (item) { map[item.n] = (item[key] - 1) * 100; });
      return xs.map(function (x) { return map[x] === undefined ? null : map[x]; });
    }
    metaChart.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 52, right: 30, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({}, {
        trigger: 'axis',
        appendToBody: true,
        backgroundColor: bg2,
        borderColor: rule,
        textStyle: { color: ink, fontSize: 12 }
      }),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: xs,
        name: '已发现数量',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: '全局加成 %',
        nameGap: 14,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: muted, fontSize: 11, formatter: '{value}%' }
      }),
      series: [
        {
          name: '突变图鉴（产率）',
          type: 'line',
          step: 'end',
          symbol: 'circle',
          symbolSize: 6,
          lineStyle: { color: accent, width: 2 },
          itemStyle: { color: accent },
          data: seriesFrom(E.codexM, 'atp')
        },
        {
          name: '物种图鉴（Biomass）',
          type: 'line',
          step: 'end',
          symbol: 'circle',
          symbolSize: 6,
          lineStyle: { color: accent2, width: 2 },
          itemStyle: { color: accent2 },
          data: seriesFrom(E.codexS, 'bio')
        },
        {
          name: '生态韧性（产率）',
          type: 'line',
          step: 'end',
          symbol: 'circle',
          symbolSize: 6,
          lineStyle: { color: palette[2], width: 2 },
          itemStyle: { color: palette[2] },
          data: E.resilience.map(function (r) { return (r.mult - 1) * 100; })
        }
      ]
    });
    window.addEventListener('resize', function () { metaChart.resize(); });
  }
})();

/* ---------------- 上限翻倍实验：EP 构成与敏感性对照 ---------------- */
(function () {
  var C = window.CAP2;
  if (!C || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();
  var palette = [accent, accent2, '#c9a227', '#6b8fbf', '#8a6bbf'];

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var breakdownEl = document.getElementById('chart-cap-ep');
  if (breakdownEl) {
    var chart = echarts.init(breakdownEl, null, { renderer: 'svg' });
    var names = C.breakdown.new.map(function (d) { return d.name; });
    chart.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 52, right: 24, top: 40, bottom: 34 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: ['旧内容（实测 18）', '新内容（理论 36）'],
        axisLabel: { color: ink, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: 'EP',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: names.map(function (name, i) {
        return {
          name: name,
          type: 'bar',
          stack: 'total',
          barMaxWidth: 64,
          itemStyle: { color: palette[i], opacity: 0.92 },
          label: { show: true, color: '#fff', fontSize: 10, formatter: '{c}' },
          data: [C.breakdown.old[i].value, C.breakdown.new[i].value]
        };
      })
    });
    window.addEventListener('resize', function () { chart.resize(); });
  }

  var scenarioLabels = C.scenarios.map(function (s) { return s.label; });
  var epCategories = ['旧基准'].concat(scenarioLabels);
  var epBoxes = [C.oldBase.ep.box].concat(C.scenarios.map(function (s) { return s.ep.box; }));

  var epBoxEl = document.getElementById('chart-cap-epbox');
  if (epBoxEl) {
    var epBox = echarts.init(epBoxEl, null, { renderer: 'svg' });
    epBox.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 48, right: 24, top: 28, bottom: 60 },
      tooltip: Object.assign({}, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: epCategories,
        axisLabel: { color: ink, fontSize: 10, interval: 0, rotate: 18 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: 'EP',
        min: 14,
        max: 36,
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'boxplot',
        data: epBoxes,
        boxWidth: [22, 14, 14, 14, 14, 14],
        itemStyle: { color: 'rgba(255,255,255,0.78)', borderColor: accent, borderWidth: 1.4 },
        markLine: {
          silent: true,
          symbol: 'none',
          lineStyle: { color: accent2, type: 'dashed', width: 1.4 },
          label: { color: accent2, fontSize: 10, formatter: '理论上限 36' },
          data: [{ yAxis: 36 }]
        }
      }]
    });
    window.addEventListener('resize', function () { epBox.resize(); });
  }

  var bioEl = document.getElementById('chart-cap-biobox');
  if (bioEl) {
    var bioBox = echarts.init(bioEl, null, { renderer: 'svg' });
    var bioBoxes = [C.oldBase.biomass.box].concat(C.scenarios.map(function (s) { return s.biomass.box; }));
    bioBox.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 62, right: 24, top: 28, bottom: 60 },
      tooltip: Object.assign({}, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: epCategories,
        axisLabel: { color: ink, fontSize: 10, interval: 0, rotate: 18 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'log',
        name: '最高 Biomass',
        nameGap: 14,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: {
          color: muted,
          fontSize: 10,
          formatter: function (v) {
            if (v >= 1000000) { return (v / 1000000).toFixed(1) + 'M'; }
            if (v >= 1000) { return (v / 1000).toFixed(0) + 'k'; }
            return v;
          }
        }
      }),
      series: [{
        type: 'boxplot',
        data: bioBoxes,
        boxWidth: [22, 14, 14, 14, 14, 14],
        itemStyle: { color: 'rgba(255,255,255,0.78)', borderColor: accent2, borderWidth: 1.4 },
        markLine: {
          silent: true,
          symbol: 'none',
          lineStyle: { color: accent, type: 'dashed', width: 1.4 },
          label: { color: accent, fontSize: 10, formatter: '里程碑顶档 100k' },
          data: [{ yAxis: 100000 }]
        }
      }]
    });
    window.addEventListener('resize', function () { bioBox.resize(); });
  }
})();

/* ---------------- 第二轮调参：调参前后与稳健性 ---------------- */
(function () {
  var T = window.TUNED;
  if (!T || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var baEl = document.getElementById('chart-tuned-ba');
  if (baEl) {
    var ba = echarts.init(baEl, null, { renderer: 'svg' });
    ba.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 64, top: 40, bottom: 34 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: ['调参前', '调参后'],
        axisLabel: { color: ink, fontSize: 12 },
        splitLine: { show: false }
      }),
      yAxis: [
        Object.assign({}, axisCommon, {
          type: 'value', name: 'EP', nameGap: 12,
          nameTextStyle: { color: muted, fontSize: 11 }, min: 0, max: 36
        }),
        Object.assign({}, axisCommon, {
          type: 'value', name: 'Biomass', nameGap: 14,
          nameTextStyle: { color: muted, fontSize: 11 }, min: 0
        })
      ],
      series: [
        {
          name: '平均 EP', type: 'bar', barMaxWidth: 56,
          itemStyle: { color: accent, opacity: 0.9 },
          label: { show: true, position: 'top', color: ink, fontSize: 11, formatter: '{c}' },
          data: [T.before.ep.mean, T.after.ep.mean]
        },
        {
          name: '平均最高 Biomass', type: 'bar', yAxisIndex: 1, barMaxWidth: 56,
          itemStyle: { color: accent2, opacity: 0.75 },
          label: {
            show: true, position: 'top', color: ink, fontSize: 10,
            formatter: function (p) { return (p.value / 1000).toFixed(0) + 'k'; }
          },
          data: [T.before.biomass.mean, T.after.biomass.mean]
        }
      ]
    });
    window.addEventListener('resize', function () { ba.resize(); });
  }

  var labels = ['调参前'].concat(T.scenarios.map(function (s) { return s.label; }));
  var epBoxes = [T.before.ep.box].concat(T.scenarios.map(function (s) { return s.ep.box; }));
  var bioBoxes = [T.before.biomass.box].concat(T.scenarios.map(function (s) { return s.biomass.box; }));

  var epEl = document.getElementById('chart-tuned-ep');
  if (epEl) {
    var ep = echarts.init(epEl, null, { renderer: 'svg' });
    ep.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 48, right: 24, top: 28, bottom: 62 },
      tooltip: Object.assign({}, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: labels,
        axisLabel: { color: ink, fontSize: 10, interval: 0, rotate: 18 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: 'EP', min: 28, max: 36, nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'boxplot', data: epBoxes, boxWidth: [22, 14, 14, 14, 14, 14],
        itemStyle: { color: 'rgba(255,255,255,0.78)', borderColor: accent, borderWidth: 1.4 },
        markLine: {
          silent: true, symbol: 'none',
          lineStyle: { color: accent2, type: 'dashed', width: 1.4 },
          label: { color: accent2, fontSize: 10, formatter: '理论上限 36' },
          data: [{ yAxis: 36 }]
        }
      }]
    });
    window.addEventListener('resize', function () { ep.resize(); });
  }

  var bioEl = document.getElementById('chart-tuned-bio');
  if (bioEl) {
    var bio = echarts.init(bioEl, null, { renderer: 'svg' });
    bio.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 64, right: 24, top: 28, bottom: 62 },
      tooltip: Object.assign({}, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: labels,
        axisLabel: { color: ink, fontSize: 10, interval: 0, rotate: 18 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'log', name: '最高 Biomass', nameGap: 14,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: {
          color: muted, fontSize: 10,
          formatter: function (v) {
            if (v >= 1000000) { return (v / 1000000).toFixed(1) + 'M'; }
            if (v >= 1000) { return (v / 1000).toFixed(0) + 'k'; }
            return v;
          }
        }
      }),
      series: [{
        type: 'boxplot', data: bioBoxes, boxWidth: [22, 14, 14, 14, 14, 14],
        itemStyle: { color: 'rgba(255,255,255,0.78)', borderColor: accent2, borderWidth: 1.4 }
      }]
    });
    window.addEventListener('resize', function () { bio.resize(); });
  }
})();

/* ---------------- 第三轮：EP 34 / 36 ---------------- */
(function () {
  var R = window.ROUND3;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var ladderEl = document.getElementById('chart-round3-ladder');
  if (ladderEl) {
    var ladder = echarts.init(ladderEl, null, { renderer: 'svg' });
    var parts = [
      { name: 'E 阶段', key: 'E', color: accent },
      { name: 'M 突变', key: 'M', color: accent2 },
      { name: 'S 物种', key: 'S', color: '#8a9a92' },
      { name: 'C 危机', key: 'C', color: '#c9a227' },
      { name: 'B Biomass', key: 'B', color: '#3a5a8c' }
    ];
    var series = parts.map(function (part) {
      return {
        name: part.name,
        type: 'bar',
        stack: 'ep',
        barMaxWidth: 68,
        itemStyle: { color: part.color },
        emphasis: { focus: 'series' },
        data: R.ladder.map(function (row) { return row[part.key]; })
      };
    });
    series.push({
      name: '合计',
      type: 'bar',
      stack: 'ep',
      silent: true,
      itemStyle: { color: 'transparent' },
      label: {
        show: true,
        position: 'top',
        color: ink,
        fontSize: 12,
        fontWeight: 700,
        formatter: function (p) { return R.ladder[p.dataIndex].total.toFixed(1); }
      },
      data: R.ladder.map(function () { return 0; })
    });
    ladder.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 48, right: 24, top: 52, bottom: 34 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({
        trigger: 'axis',
        axisPointer: { type: 'shadow' },
        formatter: function (items) {
          var row = R.ladder[items[0].dataIndex];
          return row.label + ' · ' + row.era + '<br/>合计 ' + row.total.toFixed(1) +
            ' = 10 + ' + row.M + ' + ' + row.S + ' + ' + row.C + ' + ' + row.B;
        }
      }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.ladder.map(function (row) { return row.label; }),
        axisLabel: { color: ink, fontSize: 12 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: 'EP',
        min: 0,
        max: 40,
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: series
    });
    window.addEventListener('resize', function () { ladder.resize(); });
  }

  var bioEl = document.getElementById('chart-round3-bio');
  if (bioEl) {
    var bio = echarts.init(bioEl, null, { renderer: 'svg' });
    bio.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 64, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.seeds.map(function (s) { return '#' + s.index; }),
        axisLabel: { color: muted, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'log',
        name: '最高 Biomass',
        nameGap: 14,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: {
          color: muted,
          fontSize: 10,
          formatter: function (v) {
            if (v >= 1000000) { return (v / 1000000).toFixed(1) + 'M'; }
            if (v >= 1000) { return (v / 1000).toFixed(0) + 'k'; }
            return v;
          }
        }
      }),
      series: [
        {
          name: '纪元 1',
          type: 'bar',
          barMaxWidth: 22,
          itemStyle: { color: accent, opacity: 0.5 },
          data: R.seeds.map(function (s) { return s.bio1; })
        },
        {
          name: '纪元 2',
          type: 'bar',
          barMaxWidth: 22,
          itemStyle: { color: accent2, opacity: 0.85 },
          data: R.seeds.map(function (s) { return s.bio2; })
        }
      ]
    });
    window.addEventListener('resize', function () { bio.resize(); });
  }
})();

/* ---------------- 第十二轮：阶段 11 与危机标定 ---------------- */
(function () {
  var R = window.ROUND12;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var stageEl = document.getElementById('chart-round12-stage');
  if (stageEl) {
    var stage = echarts.init(stageEl, null, { renderer: 'svg' });
    var buckets = [];
    for (var s = 1; s <= 11; ++s) { buckets.push(s); }
    var countFor = function (list, value) {
      for (var i = 0; i < list.length; ++i) {
        if (list[i].value === value) { return list[i].count; }
      }
      return 0;
    };
    stage.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: buckets.map(String), name: '阶段', nameGap: 10,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 11 }, splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: '种子数', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [
        { name: '纪元 1', type: 'bar', barMaxWidth: 18,
          itemStyle: { color: accent, opacity: 0.55 },
          data: buckets.map(function (x) { return countFor(R.scan.stage.era1, x); }) },
        { name: '纪元 3', type: 'bar', barMaxWidth: 18,
          itemStyle: { color: '#3a5a8c', opacity: 0.85 },
          data: buckets.map(function (x) { return countFor(R.scan.stage.era3, x); }) }
      ]
    });
    window.addEventListener('resize', function () { stage.resize(); });
  }

  var crisisEl = document.getElementById('chart-round12-crisis');
  if (crisisEl) {
    var crisis = echarts.init(crisisEl, null, { renderer: 'svg' });
    var names = {1: 'Warming', 2: 'Cooling', 3: 'Low Light', 4: 'Oxygen', 5: 'Toxic',
                 6: 'Meteor', 13: 'Smog'};
    crisis.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 30, bottom: 44 },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.scan.crisis.per_type.map(function (x) { return names[x.env] || ('#' + x.env); }),
        name: '危机类型（纪元 1）', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 10 }, splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: '通过率 %', min: 0, max: 100, nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'bar', barMaxWidth: 26,
        itemStyle: { color: accent2, opacity: 0.8 },
        data: R.scan.crisis.per_type.map(function (x) { return x.rate; }),
        markLine: {
          symbol: 'none',
          lineStyle: { color: muted, type: 'dashed' },
          label: { color: muted, fontSize: 10 },
          data: [{ yAxis: 40, label: { formatter: '下限 40%' } },
                 { yAxis: 65, label: { formatter: '上限 65%' } }]
        }
      }]
    });
    window.addEventListener('resize', function () { crisis.resize(); });
  }

  var tuneEl = document.getElementById('chart-round12-tuning');
  if (tuneEl) {
    var tune = echarts.init(tuneEl, null, { renderer: 'svg' });
    tune.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 64, top: 44, bottom: 64 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: R.tuning.map(function (t) { return t.step; }),
        axisLabel: { color: ink, fontSize: 10, rotate: 18 }, splitLine: { show: false }
      }),
      yAxis: [
        Object.assign({}, axisCommon, {
          type: 'value', name: '6/6 通过率 %', min: 0, max: 100, nameGap: 12,
          nameTextStyle: { color: muted, fontSize: 11 }
        }),
        Object.assign({}, axisCommon, {
          type: 'value', name: '首分化 p50（s）', min: 180, max: 320, nameGap: 14,
          nameTextStyle: { color: muted, fontSize: 11 }, splitLine: { show: false }
        })
      ],
      series: [
        { name: '6/6 危机通过率', type: 'line', symbolSize: 7,
          itemStyle: { color: accent }, lineStyle: { color: accent, width: 2 },
          data: R.tuning.map(function (t) { return t.pass6; }) },
        { name: '首分化 p50', type: 'line', yAxisIndex: 1, symbolSize: 7,
          itemStyle: { color: accent2 }, lineStyle: { color: accent2, width: 2 },
          data: R.tuning.map(function (t) { return t.p50; }) }
      ]
    });
    window.addEventListener('resize', function () { tune.resize(); });
  }

  var coverEl = document.getElementById('chart-round12-coverage');
  if (coverEl) {
    var cover = echarts.init(coverEl, null, { renderer: 'svg' });
    cover.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 36, bottom: 44 },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.scan.coverage.species.map(function (r) { return '#' + r.id; }),
        name: '物种编号', nameGap: 12, nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 10 }, splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: '出现的种子数', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'bar', barMaxWidth: 18,
        itemStyle: { color: accent2, opacity: 0.75 },
        data: R.scan.coverage.species.map(function (r) { return r.count; })
      }]
    });
    window.addEventListener('resize', function () { cover.resize(); });
  }

  /* 验收表由 make_report_table.py 生成，这里只负责渲染，正文不手写数字 */
  var body = document.getElementById('acceptance-body');
  if (body && window.ACCEPTANCE) {
    var html = '';
    window.ACCEPTANCE.rows.forEach(function (row) {
      var tag = row.status === '达标' ? 'ok' : 'warn';
      html += '<tr><td>' + row.item + '</td><td class="num">' + row.target +
              '</td><td class="num">' + row.actual + '</td><td><span class="tag ' + tag +
              '">' + row.status + '</span></td><td class="num"><code>' + row.source +
              '</code></td></tr>';
    });
    body.innerHTML = html;
  }
})();

/* ---------------- 第十一轮：结构修复（ATP/危机/突变定价） ---------------- */
(function () {
  var R = window.ROUND11;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var crisisNames = {
    1: 'Warming', 2: 'Cooling', 3: 'Low Light', 4: 'Oxygen', 5: 'Toxic', 6: 'Meteor',
    13: 'Smog'
  };

  var crisisEl = document.getElementById('chart-round11-crisis');
  if (crisisEl) {
    var crisis = echarts.init(crisisEl, null, { renderer: 'svg' });
    var ids = R.scan.crisis.per_type['1'].map(function (x) { return x.env; });
    var rateFor = function (era, env) {
      var list = R.scan.crisis.per_type[era];
      for (var i = 0; i < list.length; ++i) {
        if (list[i].env === env) { return list[i].rate; }
      }
      return 0;
    };
    crisis.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 40, bottom: 44 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: ids.map(function (id) { return crisisNames[id] || ('#' + id); }),
        name: '危机类型', nameGap: 12, nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 10 }, splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: '通过率 %', max: 100, nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [
        { name: '纪元 1', type: 'bar', barMaxWidth: 16,
          itemStyle: { color: accent, opacity: 0.7 },
          data: ids.map(function (id) { return rateFor('1', id); }) },
        { name: '纪元 2', type: 'bar', barMaxWidth: 16,
          itemStyle: { color: accent2, opacity: 0.7 },
          data: ids.map(function (id) { return rateFor('2', id); }) }
      ]
    });
    window.addEventListener('resize', function () { crisis.resize(); });
  }

  var epEl = document.getElementById('chart-round11-ep');
  if (epEl) {
    var ep = echarts.init(epEl, null, { renderer: 'svg' });
    ep.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 36, bottom: 40 },
      tooltip: Object.assign({}, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: ['纪元 1', '纪元 2', '纪元 3'],
        axisLabel: { color: ink, fontSize: 12 }, splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: 'EP', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'boxplot', boxWidth: [28, 28, 28],
        itemStyle: { color: 'rgba(255,255,255,0.78)', borderColor: accent, borderWidth: 1.4 },
        data: [R.scan.ep.era1.box, R.scan.ep.era2.box, R.scan.ep.era3.box]
      }]
    });
    window.addEventListener('resize', function () { ep.resize(); });
  }

  var gridEl = document.getElementById('chart-round11-grid');
  if (gridEl) {
    var grid = echarts.init(gridEl, null, { renderer: 'svg' });
    grid.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 64, top: 40, bottom: 44 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.grid_growth.map(function (g) { return '×' + g.growth.toFixed(2); }),
        name: '必需链成本增长', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 11 }, splitLine: { show: false }
      }),
      yAxis: [
        Object.assign({}, axisCommon, {
          type: 'value', name: '阶段 / 通过率', max: 100, nameGap: 12,
          nameTextStyle: { color: muted, fontSize: 11 }
        }),
        Object.assign({}, axisCommon, {
          type: 'value', name: '纪元 3 到 10 %', max: 100, nameGap: 14,
          nameTextStyle: { color: muted, fontSize: 11 }, splitLine: { show: false }
        })
      ],
      series: [
        { name: '纪元 1 阶段（×10）', type: 'line', symbolSize: 7,
          itemStyle: { color: accent }, lineStyle: { color: accent, width: 2 },
          data: R.grid_growth.map(function (g) { return g.stage1 * 10; }) },
        { name: '纪元 1 危机 ≥6 通过率', type: 'line', symbolSize: 7,
          itemStyle: { color: accent2 }, lineStyle: { color: accent2, width: 2 },
          data: R.grid_growth.map(function (g) { return g.pass1; }) },
        { name: '纪元 3 到阶段 10', type: 'bar', yAxisIndex: 1, barMaxWidth: 20,
          itemStyle: { color: '#3a5a8c', opacity: 0.45 },
          data: R.grid_growth.map(function (g) { return g.top3; }) }
      ]
    });
    window.addEventListener('resize', function () { grid.resize(); });
  }

  var coverEl = document.getElementById('chart-round11-coverage');
  if (coverEl) {
    var cover = echarts.init(coverEl, null, { renderer: 'svg' });
    var rows = R.scan.coverage.species_counts;
    cover.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 36, bottom: 44 },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: rows.map(function (r) { return '#' + r.id; }),
        name: '物种编号', nameGap: 12, nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 10 }, splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: '出现的种子数', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'bar', barMaxWidth: 18,
        itemStyle: { color: accent2, opacity: 0.75 },
        data: rows.map(function (r) { return r.count; })
      }]
    });
    window.addEventListener('resize', function () { cover.resize(); });
  }
})();

/* ---------------- 第十轮：三段标定（前段 / 中后段 / 理念） ---------------- */
(function () {
  var R = window.ROUND10;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var stageEl = document.getElementById('chart-round10-stage');
  if (stageEl) {
    var stage = echarts.init(stageEl, null, { renderer: 'svg' });
    var buckets = [];
    for (var s = 1; s <= 10; ++s) { buckets.push(s); }
    var countFor = function (list, value) {
      for (var i = 0; i < list.length; ++i) {
        if (list[i].value === value) { return list[i].count; }
      }
      return 0;
    };
    stage.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: buckets.map(String), name: '阶段', nameGap: 10,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 11 }, splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: '种子数', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [
        { name: '纪元 1', type: 'bar', barMaxWidth: 18,
          itemStyle: { color: accent, opacity: 0.55 },
          data: buckets.map(function (x) { return countFor(R.scan.stage.era1, x); }) },
        { name: '纪元 2', type: 'bar', barMaxWidth: 18,
          itemStyle: { color: accent2, opacity: 0.75 },
          data: buckets.map(function (x) { return countFor(R.scan.stage.era2, x); }) },
        { name: '纪元 3', type: 'bar', barMaxWidth: 18,
          itemStyle: { color: '#3a5a8c', opacity: 0.85 },
          data: buckets.map(function (x) { return countFor(R.scan.stage.era3, x); }) }
      ]
    });
    window.addEventListener('resize', function () { stage.resize(); });
  }

  var epEl = document.getElementById('chart-round10-ep');
  if (epEl) {
    var ep = echarts.init(epEl, null, { renderer: 'svg' });
    ep.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 36, bottom: 40 },
      tooltip: Object.assign({}, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: ['纪元 1', '纪元 2', '纪元 3'],
        axisLabel: { color: ink, fontSize: 12 }, splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: 'EP', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'boxplot', boxWidth: [28, 28, 28],
        itemStyle: { color: 'rgba(255,255,255,0.78)', borderColor: accent, borderWidth: 1.4 },
        data: [R.scan.ep.era1.box, R.scan.ep.era2.box, R.scan.ep.era3.box]
      }]
    });
    window.addEventListener('resize', function () { ep.resize(); });
  }

  var gridEl = document.getElementById('chart-round10-grid');
  if (gridEl) {
    var grid = echarts.init(gridEl, null, { renderer: 'svg' });
    var xs = R.grid_growth.map(function (g) { return '×' + g.growth.toFixed(2); });
    grid.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 64, top: 40, bottom: 44 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: xs, name: '突变成本增长', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 11 }, splitLine: { show: false }
      }),
      yAxis: [
        Object.assign({}, axisCommon, {
          type: 'value', name: '阶段中位', min: 0, max: 10, nameGap: 12,
          nameTextStyle: { color: muted, fontSize: 11 }
        }),
        Object.assign({}, axisCommon, {
          type: 'value', name: '纪元 3 到 10 比例 %', min: 0, max: 100, nameGap: 14,
          nameTextStyle: { color: muted, fontSize: 11 }, splitLine: { show: false }
        })
      ],
      series: [
        { name: '纪元 1 阶段', type: 'line', symbolSize: 7,
          itemStyle: { color: accent }, lineStyle: { color: accent, width: 2 },
          data: R.grid_growth.map(function (g) { return g.stage1; }) },
        { name: '纪元 2 阶段', type: 'line', symbolSize: 7,
          itemStyle: { color: accent2 }, lineStyle: { color: accent2, width: 2 },
          data: R.grid_growth.map(function (g) { return g.stage2; }) },
        { name: '纪元 3 到阶段 10', type: 'bar', yAxisIndex: 1, barMaxWidth: 20,
          itemStyle: { color: '#3a5a8c', opacity: 0.45 },
          data: R.grid_growth.map(function (g) { return g.top3; }) }
      ]
    });
    window.addEventListener('resize', function () { grid.resize(); });
  }

  var subsEl = document.getElementById('chart-round10-subs');
  if (subsEl) {
    var subs = echarts.init(subsEl, null, { renderer: 'svg' });
    var sb = [];
    for (var i = 0; i <= 15; ++i) { sb.push(i); }
    var subCount = function (v) {
      for (var j = 0; j < R.scan.subs.dist_era3.length; ++j) {
        if (R.scan.subs.dist_era3[j].value === v) { return R.scan.subs.dist_era3[j].count; }
      }
      return 0;
    };
    subs.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 36, bottom: 40 },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category', data: sb.map(String), name: '纪元 3 理念升级数', nameGap: 10,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 11 }, splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value', name: '种子数', nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'bar', barMaxWidth: 22,
        itemStyle: { color: accent2, opacity: 0.8 },
        data: sb.map(subCount)
      }]
    });
    window.addEventListener('resize', function () { subs.resize(); });
  }
})();

/* ---------------- 第九轮：理念与子升级 ---------------- */
(function () {
  var R = window.ROUND9;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var epEl = document.getElementById('chart-round9-ep');
  if (epEl) {
    var ep = echarts.init(epEl, null, { renderer: 'svg' });
    ep.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 36, bottom: 40 },
      tooltip: Object.assign({}, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: ['纪元 1', '纪元 2', '纪元 3'],
        axisLabel: { color: ink, fontSize: 12 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: 'EP',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'boxplot',
        boxWidth: [28, 28, 28],
        itemStyle: { color: 'rgba(255,255,255,0.78)', borderColor: accent, borderWidth: 1.4 },
        data: [R.scan.ep.era1.box, R.scan.ep.era2.box, R.scan.ep.era3.box]
      }]
    });
    window.addEventListener('resize', function () { ep.resize(); });
  }

  var tierEl = document.getElementById('chart-round9-tiers');
  if (tierEl) {
    var tier = echarts.init(tierEl, null, { renderer: 'svg' });
    var speciesColor = {
      'Agriculture': accent, 'Writing': accent, 'Philosophy': accent,
      'Steam Power': accent2, 'Electricity': accent2, 'Computing': accent2,
      'Printing Press': '#7a6a3a', 'Scientific Method': '#7a6a3a', 'Mass Media': '#7a6a3a',
      'Internet': '#3a5a8c', 'Automation': '#3a5a8c', 'Fusion Power': '#3a5a8c',
      'Nanotechnology': '#5a3a6a', 'Quantum Computing': '#5a3a6a', 'Self-Replication': '#5a3a6a'
    };
    tier.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 130, right: 60, top: 20, bottom: 40 },
      tooltip: Object.assign({}, tooltipBase, {
        formatter: function (p) {
          var item = R.tier_timeline[p.dataIndex];
          return item.name + '<br/>购买时点中位数 ' + item.median_t + 's（' + item.n + ' 个种子）';
        }
      }),
      xAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: '购买时点（秒）',
        nameGap: 10,
        nameTextStyle: { color: muted, fontSize: 11 },
        min: 350,
        max: 620
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.tier_timeline.map(function (t) { return t.name; }),
        axisLabel: { color: ink, fontSize: 11 },
        splitLine: { show: false }
      }),
      series: [{
        type: 'bar',
        barMaxWidth: 14,
        data: R.tier_timeline.map(function (t) {
          return { value: t.median_t, itemStyle: { color: speciesColor[t.name] || accent } };
        }),
        markLine: {
          symbol: 'none',
          lineStyle: { color: muted, type: 'dashed' },
          label: { color: muted, fontSize: 10, formatter: '纪元结束 600s' },
          data: [{ xAxis: 600 }]
        }
      }]
    });
    window.addEventListener('resize', function () { tier.resize(); });
  }

  var subsEl = document.getElementById('chart-round9-subs');
  if (subsEl) {
    var subs = echarts.init(subsEl, null, { renderer: 'svg' });
    var buckets = [];
    for (var s = 0; s <= 15; ++s) { buckets.push(s); }
    var countFor = function (value) {
      for (var i = 0; i < R.scan.subs.era3_dist.length; ++i) {
        if (R.scan.subs.era3_dist[i].value === value) { return R.scan.subs.era3_dist[i].count; }
      }
      return 0;
    };
    subs.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 36, bottom: 40 },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: buckets.map(String),
        name: '纪元 3 购买层数',
        nameGap: 10,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: '种子数',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'bar',
        barMaxWidth: 22,
        itemStyle: { color: accent2, opacity: 0.8 },
        data: buckets.map(countFor)
      }]
    });
    window.addEventListener('resize', function () { subs.resize(); });
  }
})();

/* ---------------- 第八轮：长线进化到奇点 ---------------- */
(function () {
  var R = window.ROUND8;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var stageEl = document.getElementById('chart-round8-stage');
  if (stageEl) {
    var stage = echarts.init(stageEl, null, { renderer: 'svg' });
    var buckets = [];
    for (var s = 1; s <= 10; ++s) { buckets.push(s); }
    var countFor = function (list, value) {
      for (var i = 0; i < list.length; ++i) {
        if (list[i].value === value) { return list[i].count; }
      }
      return 0;
    };
    stage.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: buckets.map(String),
        name: '阶段',
        nameGap: 10,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: '种子数',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [
        { name: '纪元 1', type: 'bar', barMaxWidth: 18,
          itemStyle: { color: accent, opacity: 0.55 },
          data: buckets.map(function (x) { return countFor(R.scan.stage.era1, x); }) },
        { name: '纪元 2', type: 'bar', barMaxWidth: 18,
          itemStyle: { color: accent2, opacity: 0.75 },
          data: buckets.map(function (x) { return countFor(R.scan.stage.era2, x); }) },
        { name: '纪元 3', type: 'bar', barMaxWidth: 18,
          itemStyle: { color: '#3a5a8c', opacity: 0.85 },
          data: buckets.map(function (x) { return countFor(R.scan.stage.era3, x); }) }
      ]
    });
    window.addEventListener('resize', function () { stage.resize(); });
  }

  var epEl = document.getElementById('chart-round8-ep');
  if (epEl) {
    var ep = echarts.init(epEl, null, { renderer: 'svg' });
    ep.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 36, bottom: 40 },
      tooltip: Object.assign({}, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: ['纪元 1', '纪元 2', '纪元 3'],
        axisLabel: { color: ink, fontSize: 12 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: 'EP',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [{
        type: 'boxplot',
        boxWidth: [28, 28, 28],
        itemStyle: { color: 'rgba(255,255,255,0.78)', borderColor: accent, borderWidth: 1.4 },
        data: [R.scan.ep.era1.box, R.scan.ep.era2.box, R.scan.ep.era3.box]
      }]
    });
    window.addEventListener('resize', function () { ep.resize(); });
  }
})();

/* ---------------- 第七轮：节奏、方差与阶段 7 ---------------- */
(function () {
  var R = window.ROUND7;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var epEl = document.getElementById('chart-round7-ep');
  if (epEl) {
    var ep = echarts.init(epEl, null, { renderer: 'svg' });
    var buckets = [];
    for (var v = 10; v <= 45; ++v) { buckets.push(v); }
    var countFor = function (list, value) {
      for (var i = 0; i < list.length; ++i) {
        if (list[i].value === value) { return list[i].count; }
      }
      return 0;
    };
    ep.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: buckets.map(String),
        name: 'EP',
        nameGap: 10,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 9, interval: 1 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: '种子数',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [
        {
          name: '纪元 1',
          type: 'bar',
          barMaxWidth: 12,
          itemStyle: { color: accent, opacity: 0.7 },
          data: buckets.map(function (x) { return countFor(R.scan.era1.hist, x); })
        },
        {
          name: '纪元 2',
          type: 'bar',
          barMaxWidth: 12,
          itemStyle: { color: accent2, opacity: 0.85 },
          data: buckets.map(function (x) { return countFor(R.scan.era2.hist, x); })
        }
      ]
    });
    window.addEventListener('resize', function () { ep.resize(); });
  }

  var lastEl = document.getElementById('chart-round7-last');
  if (lastEl) {
    var last = echarts.init(lastEl, null, { renderer: 'svg' });
    last.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 60, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.seeds.map(function (s) { return '#' + s.index; }),
        axisLabel: { color: muted, fontSize: 10 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: '秒',
        min: 480,
        max: 600,
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [
        {
          name: '纪元 1',
          type: 'bar',
          barMaxWidth: 24,
          itemStyle: { color: accent, opacity: 0.7 },
          data: R.seeds.map(function (s) { return s.last1; }),
          markArea: {
            silent: true,
            itemStyle: { color: 'rgba(30,107,72,0.08)' },
            label: { color: muted, fontSize: 10, formatter: '目标 540–600s' },
            data: [[{ yAxis: 540 }, { yAxis: 600 }]]
          }
        },
        {
          name: '纪元 2',
          type: 'bar',
          barMaxWidth: 24,
          itemStyle: { color: accent2, opacity: 0.85 },
          data: R.seeds.map(function (s) { return s.last2; })
        }
      ]
    });
    window.addEventListener('resize', function () { last.resize(); });
  }
})();

/* ---------------- 第六轮：事件与路线第二阶段 ---------------- */
(function () {
  var R = window.ROUND6;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var ladderEl = document.getElementById('chart-round6-ladder');
  if (ladderEl) {
    var ladder = echarts.init(ladderEl, null, { renderer: 'svg' });
    var parts = [
      { name: 'E 阶段', key: 'E', color: accent },
      { name: 'M 突变', key: 'M', color: accent2 },
      { name: 'S 物种', key: 'S', color: '#8a9a92' },
      { name: 'C 危机', key: 'C', color: '#c9a227' },
      { name: 'B Biomass', key: 'B', color: '#3a5a8c' }
    ];
    var series = parts.map(function (part) {
      return {
        name: part.name,
        type: 'bar',
        stack: 'ep',
        barMaxWidth: 68,
        itemStyle: { color: part.color },
        emphasis: { focus: 'series' },
        data: R.ladder.map(function (row) { return row[part.key]; })
      };
    });
    series.push({
      name: '合计',
      type: 'bar',
      stack: 'ep',
      silent: true,
      itemStyle: { color: 'transparent' },
      label: {
        show: true,
        position: 'top',
        color: ink,
        fontSize: 12,
        fontWeight: 700,
        formatter: function (p) { return R.ladder[p.dataIndex].total.toFixed(0); }
      },
      data: R.ladder.map(function () { return 0; })
    });
    ladder.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 48, right: 24, top: 52, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({
        trigger: 'axis',
        axisPointer: { type: 'shadow' },
        formatter: function (items) {
          var row = R.ladder[items[0].dataIndex];
          return row.label + '<br/>合计 ' + row.total +
            ' = 10 + ' + row.M + ' + ' + row.S + ' + ' + row.C + ' + ' + row.B;
        }
      }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.ladder.map(function (row) { return row.label; }),
        axisLabel: { color: ink, fontSize: 11, interval: 0, rotate: 12 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: 'EP',
        min: 0,
        max: 44,
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: series
    });
    window.addEventListener('resize', function () { ladder.resize(); });
  }

  var histEl = document.getElementById('chart-round6-hist');
  if (histEl) {
    var hist = echarts.init(histEl, null, { renderer: 'svg' });
    var buckets = [32, 33, 34, 39, 40];
    var countFor = function (list, ep) {
      for (var i = 0; i < list.length; ++i) {
        if (list[i].ep === ep) { return list[i].count; }
      }
      return 0;
    };
    hist.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: buckets.map(String),
        name: 'EP',
        nameGap: 10,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: '种子数',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [
        {
          name: '纪元 1',
          type: 'bar',
          barMaxWidth: 34,
          itemStyle: { color: accent, opacity: 0.7 },
          data: buckets.map(function (ep) { return countFor(R.scan.era1.hist, ep); })
        },
        {
          name: '纪元 2',
          type: 'bar',
          barMaxWidth: 34,
          itemStyle: { color: accent2, opacity: 0.85 },
          data: buckets.map(function (ep) { return countFor(R.scan.era2.hist, ep); })
        }
      ]
    });
    window.addEventListener('resize', function () { hist.resize(); });
  }
})();

/* ---------------- 第五轮：平衡重定与 1000 种子分布 ---------------- */
(function () {
  var R = window.ROUND5;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var histEl = document.getElementById('chart-round5-hist');
  if (histEl) {
    var hist = echarts.init(histEl, null, { renderer: 'svg' });
    var buckets = [31, 32, 33, 34, 35, 36, 37];
    var countFor = function (list, ep) {
      for (var i = 0; i < list.length; ++i) {
        if (list[i].ep === ep) { return list[i].count; }
      }
      return 0;
    };
    hist.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 56, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: buckets.map(String),
        name: 'EP',
        nameGap: 10,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: { color: ink, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: '种子数',
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: [
        {
          name: '纪元 1',
          type: 'bar',
          barMaxWidth: 34,
          itemStyle: { color: accent, opacity: 0.7 },
          data: buckets.map(function (ep) { return countFor(R.scan.era1.hist, ep); })
        },
        {
          name: '纪元 2',
          type: 'bar',
          barMaxWidth: 34,
          itemStyle: { color: accent2, opacity: 0.85 },
          data: buckets.map(function (ep) { return countFor(R.scan.era2.hist, ep); })
        }
      ]
    });
    window.addEventListener('resize', function () { hist.resize(); });
  }

  var bioEl = document.getElementById('chart-round5-bio');
  if (bioEl) {
    var bio = echarts.init(bioEl, null, { renderer: 'svg' });
    bio.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 68, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.seeds.map(function (s) { return '#' + s.index; }),
        axisLabel: { color: muted, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'log',
        name: '最高 Biomass',
        nameGap: 14,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: {
          color: muted,
          fontSize: 10,
          formatter: function (v) {
            if (v >= 1000000) { return (v / 1000000).toFixed(1) + 'M'; }
            if (v >= 1000) { return (v / 1000).toFixed(0) + 'k'; }
            return v;
          }
        }
      }),
      series: [
        {
          name: '纪元 1',
          type: 'bar',
          barMaxWidth: 22,
          itemStyle: { color: accent, opacity: 0.5 },
          data: R.seeds.map(function (s) { return s.bio1; })
        },
        {
          name: '纪元 2',
          type: 'bar',
          barMaxWidth: 22,
          itemStyle: { color: accent2, opacity: 0.85 },
          data: R.seeds.map(function (s) { return s.bio2; })
        }
      ]
    });
    window.addEventListener('resize', function () { bio.resize(); });
  }
})();

/* ---------------- 第四轮：内容扩容与 38 EP ---------------- */
(function () {
  var R = window.ROUND4;
  if (!R || !window.echarts) {
    return;
  }

  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  var baseText = { fontFamily: 'Bricolage, "Microsoft YaHei", sans-serif', color: ink };
  var axisCommon = {
    axisLine: { lineStyle: { color: rule } },
    axisTick: { show: false },
    axisLabel: { color: muted, fontSize: 11 },
    splitLine: { lineStyle: { color: rule, type: 'dashed', opacity: 0.7 } }
  };
  var tooltipBase = {
    appendToBody: true,
    backgroundColor: bg2,
    borderColor: rule,
    textStyle: { color: ink, fontSize: 12 }
  };

  var ladderEl = document.getElementById('chart-round4-ladder');
  if (ladderEl) {
    var ladder = echarts.init(ladderEl, null, { renderer: 'svg' });
    var parts = [
      { name: 'E 阶段', key: 'E', color: accent },
      { name: 'M 突变', key: 'M', color: accent2 },
      { name: 'S 物种', key: 'S', color: '#8a9a92' },
      { name: 'C 危机', key: 'C', color: '#c9a227' },
      { name: 'B Biomass', key: 'B', color: '#3a5a8c' }
    ];
    var series = parts.map(function (part) {
      return {
        name: part.name,
        type: 'bar',
        stack: 'ep',
        barMaxWidth: 64,
        itemStyle: { color: part.color },
        emphasis: { focus: 'series' },
        data: R.ladder.map(function (row) { return row[part.key]; })
      };
    });
    series.push({
      name: '合计',
      type: 'bar',
      stack: 'ep',
      silent: true,
      itemStyle: { color: 'transparent' },
      label: {
        show: true,
        position: 'top',
        color: ink,
        fontSize: 12,
        fontWeight: 700,
        formatter: function (p) { return R.ladder[p.dataIndex].total.toFixed(1); }
      },
      data: R.ladder.map(function () { return 0; })
    });
    ladder.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 48, right: 24, top: 52, bottom: 46 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({
        trigger: 'axis',
        axisPointer: { type: 'shadow' },
        formatter: function (items) {
          var row = R.ladder[items[0].dataIndex];
          return row.label + '<br/>合计 ' + row.total.toFixed(1) +
            ' = 10 + ' + row.M + ' + ' + row.S + ' + ' + row.C + ' + ' + row.B;
        }
      }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.ladder.map(function (row) { return row.label; }),
        axisLabel: { color: ink, fontSize: 11, interval: 0, rotate: 12 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'value',
        name: 'EP',
        min: 0,
        max: 42,
        nameGap: 12,
        nameTextStyle: { color: muted, fontSize: 11 }
      }),
      series: series
    });
    window.addEventListener('resize', function () { ladder.resize(); });
  }

  var bioEl = document.getElementById('chart-round4-bio');
  if (bioEl) {
    var bio = echarts.init(bioEl, null, { renderer: 'svg' });
    bio.setOption({
      animation: false,
      textStyle: baseText,
      grid: { left: 68, right: 24, top: 40, bottom: 40 },
      legend: { top: 0, textStyle: { color: muted, fontSize: 11 } },
      tooltip: Object.assign({ trigger: 'axis' }, tooltipBase),
      xAxis: Object.assign({}, axisCommon, {
        type: 'category',
        data: R.seeds.map(function (s) { return '#' + s.index; }),
        axisLabel: { color: muted, fontSize: 11 },
        splitLine: { show: false }
      }),
      yAxis: Object.assign({}, axisCommon, {
        type: 'log',
        name: '最高 Biomass',
        nameGap: 14,
        nameTextStyle: { color: muted, fontSize: 11 },
        axisLabel: {
          color: muted,
          fontSize: 10,
          formatter: function (v) {
            if (v >= 1000000) { return (v / 1000000).toFixed(1) + 'M'; }
            if (v >= 1000) { return (v / 1000).toFixed(0) + 'k'; }
            return v;
          }
        }
      }),
      series: [
        {
          name: '纪元 1',
          type: 'bar',
          barMaxWidth: 22,
          itemStyle: { color: accent, opacity: 0.5 },
          data: R.seeds.map(function (s) { return s.bio1; })
        },
        {
          name: '纪元 2',
          type: 'bar',
          barMaxWidth: 22,
          itemStyle: { color: accent2, opacity: 0.85 },
          data: R.seeds.map(function (s) { return s.bio2; })
        }
      ]
    });
    window.addEventListener('resize', function () { bio.resize(); });
  }
})();
