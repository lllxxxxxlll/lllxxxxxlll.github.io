// assets/charts.js
(function () {
  var style = getComputedStyle(document.documentElement);
  var accent = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink = style.getPropertyValue('--ink').trim();
  var muted = style.getPropertyValue('--muted').trim();
  var rule = style.getPropertyValue('--rule').trim();
  var bg2 = style.getPropertyValue('--bg2').trim();

  // --- Mermaid init (diagrams are declared in <pre class="mermaid"> blocks) ---
  if (window.mermaid) {
    window.mermaid.initialize({ startOnLoad: true, theme: 'neutral', securityLevel: 'loose' });
  }

  // --- Chart: 四条充值 / 订阅路径对比 (radar) ---
  var el = document.getElementById('chart-paths');
  if (el && window.echarts) {
    var chart = echarts.init(el, null, { renderer: 'svg' });

    var indicator = [
      { name: '成本友好', max: 100 },
      { name: '支付成功率', max: 100 },
      { name: '账号安全', max: 100 },
      { name: '长期稳定', max: 100 }
    ];

    var series = [
      { name: '礼品卡自充到余额', value: [60, 90, 95, 95], color: accent },
      { name: '国内万事达卡直付', value: [70, 20, 80, 30], color: accent2 },
      { name: '虚拟信用卡', value: [55, 78, 62, 68], color: muted },
      { name: '第三方低价代充', value: [95, 85, 20, 25], color: accent + '88' }
    ];

    chart.setOption({
      animation: false,
      color: series.map(function (s) { return s.color; }),
      tooltip: { trigger: 'item', appendToBody: true },
      legend: {
        bottom: 0,
        left: 'center',
        itemWidth: 12,
        itemHeight: 12,
        textStyle: { color: ink, fontSize: 12 }
      },
      radar: {
        center: ['50%', '46%'],
        radius: '62%',
        indicator: indicator,
        axisName: { color: ink, fontSize: 12 },
        axisLine: { lineStyle: { color: rule } },
        splitLine: { lineStyle: { color: rule } },
        splitArea: { show: false }
      },
      series: [{
        type: 'radar',
        symbolSize: 5,
        lineStyle: { width: 2 },
        areaStyle: { opacity: 0.12 },
        emphasis: { focus: 'series' },
        data: series.map(function (s) {
          return { name: s.name, value: s.value, lineStyle: { color: s.color }, itemStyle: { color: s.color } };
        })
      }]
    });

    window.addEventListener('resize', function () { chart.resize(); });
  }
})();
