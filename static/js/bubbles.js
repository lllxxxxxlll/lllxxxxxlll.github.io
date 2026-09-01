// 深海气泡 + 浮游光点粒子层
(function () {
  const canvas = document.getElementById("particles");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");

  let W = 0, H = 0, DPR = Math.min(window.devicePixelRatio || 1, 2);

  function resize() {
    W = canvas.clientWidth = window.innerWidth;
    H = canvas.clientHeight = window.innerHeight;
    canvas.width = W * DPR;
    canvas.height = H * DPR;
    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
  }
  resize();
  window.addEventListener("resize", resize);

  // ---- 气泡 ----
  const bubbles = [];
  const BUBBLE_COUNT = Math.min(50, Math.floor((W * H) / 30000));

  function spawnBubble() {
    return {
      x: Math.random() * W,
      y: H + Math.random() * 40,
      r: 1 + Math.random() * 4,
      vy: 0.25 + Math.random() * 0.8,
      amp: 0.3 + Math.random() * 0.6,    // 左右摇摆幅度
      freq: 0.005 + Math.random() * 0.012, // 摇摆频率
      phase: Math.random() * Math.PI * 2,
      life: 1,
      decaySpeed: 0.0015 + Math.random() * 0.0025,
    };
  }
  for (let i = 0; i < BUBBLE_COUNT; i++) {
    const b = spawnBubble();
    b.y = Math.random() * H;
    bubbles.push(b);
  }

  // ---- 浮游光点 ----
  const sparkles = [];
  const SPARKLE_COUNT = Math.min(30, Math.floor((W * H) / 50000));

  function spawnSparkle() {
    return {
      x: Math.random() * W,
      y: Math.random() * H,
      r: 0.5 + Math.random() * 1.6,
      vx: (Math.random() - 0.5) * 0.15,
      vy: (Math.random() - 0.5) * 0.1,
      twinkle: Math.random() * Math.PI * 2,
      twinkleSpeed: 0.01 + Math.random() * 0.03,
    };
  }
  for (let i = 0; i < SPARKLE_COUNT; i++) sparkles.push(spawnSparkle());

  let t = 0;
  function frame() {
    ctx.clearRect(0, 0, W, H);
    t += 1;

    // 气泡
    for (let i = 0; i < bubbles.length; i++) {
      const b = bubbles[i];
      b.y -= b.vy;
      b.x += Math.sin(t * b.freq + b.phase) * b.amp;
      if (b.y < -20) {
        // 从底部重新生成
        Object.assign(b, spawnBubble());
        b.y = H + 10;
      }

      const alpha = Math.min(1, b.y / H) * 0.55;
      ctx.beginPath();
      ctx.arc(b.x, b.y, b.r, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(180, 220, 240, ${alpha * 0.3})`;
      ctx.fill();
      ctx.strokeStyle = `rgba(200, 230, 250, ${alpha * 0.5})`;
      ctx.lineWidth = 0.5;
      ctx.stroke();

      // 高光点
      ctx.beginPath();
      ctx.arc(b.x - b.r * 0.3, b.y - b.r * 0.3, b.r * 0.25, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(255, 255, 255, ${alpha * 0.8})`;
      ctx.fill();
    }

    // 浮游光点
    for (let i = 0; i < sparkles.length; i++) {
      const s = sparkles[i];
      s.x += s.vx;
      s.y += s.vy;
      s.twinkle += s.twinkleSpeed;

      if (s.x < 0) s.x = W;
      if (s.x > W) s.x = 0;
      if (s.y < 0) s.y = H;
      if (s.y > H) s.y = 0;

      const alpha = 0.35 + Math.sin(s.twinkle) * 0.35;
      ctx.beginPath();
      ctx.arc(s.x, s.y, s.r, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(160, 220, 240, ${alpha})`;
      ctx.fill();
    }

    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
})();
