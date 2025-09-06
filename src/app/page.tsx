"use client";

import { useState, useEffect, useRef } from 'react';
import * as THREE from 'three';

export default function NationalityGame() {
  const [gameStarted, setGameStarted] = useState(false);
  const threeContainer = useRef<HTMLDivElement>(null);

  const startTest = () => {
    setGameStarted(true);
  };

  // Landing page
  if (!gameStarted) {
    return (
      <div className="min-h-screen bg-[#f7e4ba] flex flex-col items-center justify-center relative overflow-hidden">
        
        {/* Image in the top-right corner */}
        <img 
          src="/seleccion.png" 
          alt="Seleccion logo"
          className="absolute top-4 right-4 h-32 w-32 md:h-40 md:w-40 object-cover" 
        />
  
        {/* Colombia flag stripe - horizontal and elongated */}
        <div className="absolute top-1/2 -translate-y-1/2 w-full h-[15vh] flex flex-col">
          <div className="bg-[#ffb514] flex-grow-[2] w-full"></div>
          <div className="bg-[#09a3db] flex-grow-[1] w-full"></div>
          <div className="bg-[#c8102e] flex-grow-[1] w-full"></div>
        </div>
  
        <div className="text-center z-10 p-6 bg-[white] rounded-xl shadow-lg">
          <h1 className="text-4xl font-bold mb-4 text-stone-800">Nationality Verification</h1>
          <p className="text-stone-600 mb-8">A handpicked challenge that anyone from your country can complete!</p>
          <button
            onClick={startTest}
            className="bg-stone-800 text-stone-100 font-bold py-3 px-8 rounded-full transition-all duration-300 transform hover:scale-105 hover:shadow-xl"
          >
            Begin
          </button>
        </div>
      </div>
    );
  }

  // Three.js Scene Component
  return <ThreeJSScene containerRef={threeContainer} />;
}

// Three.js Scene Component
function ThreeJSScene({ containerRef }: { containerRef: React.RefObject<HTMLDivElement> }) {
  useEffect(() => {
    if (!containerRef.current) return;

    // Scene setup
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    
    renderer.setSize(window.innerWidth, window.innerHeight);
    containerRef.current.appendChild(renderer.domElement);

    // Add some objects to the scene
    const geometry = new THREE.BoxGeometry();
    const material = new THREE.MeshBasicMaterial({ color: 0x00ff00 });
    const cube = new THREE.Mesh(geometry, material);
    scene.add(cube);

    camera.position.z = 5;

    // Animation
    const animate = () => {
      requestAnimationFrame(animate);
      
      cube.rotation.x += 0.01;
      cube.rotation.y += 0.01;
      
      renderer.render(scene, camera);
    };
    
    animate();

    // Handle window resize
    const handleResize = () => {
      camera.aspect = window.innerWidth / window.innerHeight;
      camera.updateProjectionMatrix();
      renderer.setSize(window.innerWidth, window.innerHeight);
    };

    window.addEventListener('resize', handleResize);

    // Cleanup
    return () => {
      window.removeEventListener('resize', handleResize);
      containerRef.current?.removeChild(renderer.domElement);
      renderer.dispose();
    };
  }, [containerRef]);

  return (
    <div className="relative w-full h-screen">
      <div ref={containerRef} className="w-full h-full" />
      
      {/* Back button */}
      <button 
        className="absolute top-4 left-4 bg-stone-800 text-white px-4 py-2 rounded-md"
        onClick={() => window.location.reload()}
      >
        Back to Home
      </button>
      
      {/* Game instructions */}
      <div className="absolute bottom-4 left-1/2 transform -translate-x-1/2 bg-black bg-opacity-50 text-white p-4 rounded-md">
        <p>Use your knowledge to identify national symbols!</p>
      </div>
    </div>
  );
}