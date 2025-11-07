import numpy as np
import cv2
import os
import matplotlib.pyplot as plt
from datetime import datetime

print("Current Date and Time (UTC - YYYY-MM-DD HH:MM:SS formatted): 2025-11-03 09:59:25")
print("Current User's Login: JukoYao")
print("-" * 50)


class FeatureExtractorWithCentroidNormalized:
    """特征提取 - 重心归一化处理"""

    def __init__(self, input_dir=None, output_dir='extracted_features'):
        self.input_dir = input_dir
        self.output_dir = output_dir
        self.hu_moments = None
        self.areas = None
        self.centroids_normalized = None  # ⭐ 归一化坐标 (0-1)
        self.density_features = None
        self.n_samples = None
        self.image_size = None

    def find_dataset_directory(self, root_dir='.', max_depth=5):
        """Find the latest rectangle_dataset directory"""

        def search(directory, depth=0):
            if depth > max_depth:
                return None
            try:
                for item in os.listdir(directory):
                    item_path = os.path.join(directory, item)
                    if os.path.isdir(item_path) and item.startswith('rectangle_dataset_'):
                        png_files = [f for f in os.listdir(item_path) if f.endswith('.png')]
                        if len(png_files) > 0:
                            return item_path
                    if os.path.isdir(item_path) and not item.startswith('.'):
                        result = search(item_path, depth + 1)
                        if result:
                            return result
            except PermissionError:
                pass
            return None

        found_path = search(root_dir)
        if found_path:
            print(f"Found dataset: {os.path.abspath(found_path)}")
            return found_path
        return None

    def extract_hu_moments_and_area(self, image_path):
        """Extract Hu moments and area"""
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        if image is None:
            raise ValueError(f"Cannot read image: {image_path}")

        _, binary = cv2.threshold(image, 127, 255, cv2.THRESH_BINARY_INV)

        moments = cv2.moments(binary)
        hu_moments = cv2.HuMoments(moments)
        hu_moments = hu_moments.flatten()
        area = np.sum(binary > 0)

        return hu_moments, area

    def extract_centroid_normalized(self, image_path):
        """
        提取重心坐标并归一化到 [0, 1]
        """
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        if image is None:
            raise ValueError(f"Cannot read image: {image_path}")

        _, binary = cv2.threshold(image, 127, 255, cv2.THRESH_BINARY_INV)

        h, w = binary.shape
        if self.image_size is None:
            self.image_size = (h, w)

        white_pixels = np.where(binary == 255)

        if len(white_pixels[0]) == 0:
            centroid_x = w / 2.0
            centroid_y = h / 2.0
        else:
            centroid_y = np.mean(white_pixels[0])
            centroid_x = np.mean(white_pixels[1])

        # ⭐ 归一化到 [0, 1]
        centroid_x_norm = centroid_x / w
        centroid_y_norm = centroid_y / h

        return centroid_x_norm, centroid_y_norm

    def extract_density_features(self, image_path, grid_size=4):
        """Extract 4x4 grid density features"""
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        if image is None:
            raise ValueError(f"Cannot read image: {image_path}")

        _, binary = cv2.threshold(image, 127, 255, cv2.THRESH_BINARY_INV)
        binary = binary // 255

        h, w = binary.shape
        grid_h = h // grid_size
        grid_w = w // grid_size

        densities = np.zeros((grid_size, grid_size))
        for i in range(grid_size):
            for j in range(grid_size):
                grid = binary[i * grid_h:(i + 1) * grid_h, j * grid_w:(j + 1) * grid_w]
                density = np.sum(grid == 1) / (grid_h * grid_w)
                densities[i, j] = density

        return densities.flatten()

    def process_dataset(self):
        """Process entire dataset"""
        if self.input_dir is None:
            self.input_dir = self.find_dataset_directory()

        if self.input_dir is None:
            raise ValueError("Cannot find dataset directory")

        print(f"\n[Loading dataset from: {os.path.abspath(self.input_dir)}]")

        image_files = [f for f in os.listdir(self.input_dir) if f.endswith('.png')]
        image_files.sort()

        self.n_samples = len(image_files)
        print(f"Total samples: {self.n_samples}")

        hu_moments_list = []
        areas_list = []
        centroids_normalized_list = []
        density_list = []

        print(f"\n[Extracting features]")
        print("-" * 90)
        print(f"{'Sample':<10} {'Hu(7)':<12} {'Area':<10} {'Centroid_Norm(x,y)':<20} {'Density(16)':<20}")
        print("-" * 90)

        for idx, fname in enumerate(image_files):
            try:
                image_path = os.path.join(self.input_dir, fname)

                hu_moms, area = self.extract_hu_moments_and_area(image_path)
                hu_moments_list.append(hu_moms)
                areas_list.append(area)

                # ⭐ 提取归一化重心
                cx_norm, cy_norm = self.extract_centroid_normalized(image_path)
                centroids_normalized_list.append([cx_norm, cy_norm])

                density_feat = self.extract_density_features(image_path, grid_size=4)
                density_list.append(density_feat)

                if (idx + 1) % 20 == 0:
                    print(f"{idx + 1:<10} Processing...")

            except Exception as e:
                print(f"Error processing {fname}: {e}")

        self.hu_moments = np.array(hu_moments_list)
        self.areas = np.array(areas_list)
        self.centroids_normalized = np.array(centroids_normalized_list)
        self.density_features = np.array(density_list)

        print("-" * 90)
        print(f"\n[Feature Extraction Completed]")
        print(f"Hu moments shape: {self.hu_moments.shape}")
        print(f"Areas shape: {self.areas.shape}")
        print(f"Centroids (normalized) shape: {self.centroids_normalized.shape}")
        print(f"Density features shape: {self.density_features.shape}")
        print(f"Image size: {self.image_size}")

        self.layer1_features = np.concatenate([self.hu_moments, self.areas.reshape(-1, 1)], axis=1)
        print(f"\n[Layer 1 Features (Hu moments + Area)]")
        print(f"Shape: {self.layer1_features.shape}")
        print(f"Dimension: {self.layer1_features.shape[1]} (7 Hu moments + 1 area)")

        self.layer2_features = np.concatenate(
            [self.density_features, self.centroids_normalized],
            axis=1
        )
        print(f"\n[Layer 2 Features (Density + Centroid Normalized)]")
        print(f"Shape: {self.layer2_features.shape}")
        print(f"Dimension: {self.layer2_features.shape[1]} (16 grid density + 2 centroid normalized)")

        return self.layer1_features, self.layer2_features

    def compute_statistics(self):
        """Compute statistics"""
        print(f"\n[Feature Statistics]")
        print("=" * 90)

        print(f"\nLayer 1 Features (Hu moments + Area):")
        print(f"  Area mean: {np.mean(self.areas):.2f}")
        print(f"  Area std: {np.std(self.areas):.2f}")
        print(f"  Area range: [{np.min(self.areas):.0f}, {np.max(self.areas):.0f}]")

        print(f"\nLayer 2 Features (Density + Centroid Normalized):")
        print(f"  Density mean: {np.mean(self.density_features):.4f}")
        print(f"  Density std: {np.std(self.density_features):.4f}")

        print(f"\n  Centroid (Normalized [0-1]):")
        print(f"  Centroid X mean: {np.mean(self.centroids_normalized[:, 0]):.4f}")
        print(f"  Centroid X std: {np.std(self.centroids_normalized[:, 0]):.4f}")
        print(f"  Centroid X range: [{np.min(self.centroids_normalized[:, 0]):.4f}, {np.max(self.centroids_normalized[:, 0]):.4f}]")
        print(f"  Centroid Y mean: {np.mean(self.centroids_normalized[:, 1]):.4f}")
        print(f"  Centroid Y std: {np.std(self.centroids_normalized[:, 1]):.4f}")
        print(f"  Centroid Y range: [{np.min(self.centroids_normalized[:, 1]):.4f}, {np.max(self.centroids_normalized[:, 1]):.4f}]")

    def save_features(self):
        """Save features to disk"""
        os.makedirs(self.output_dir, exist_ok=True)

        print(f"\n[Saving features]")

        np.save(os.path.join(self.output_dir, 'hu_moments.npy'), self.hu_moments)
        np.save(os.path.join(self.output_dir, 'areas.npy'), self.areas)
        np.save(os.path.join(self.output_dir, 'centroids_normalized.npy'), self.centroids_normalized)
        np.save(os.path.join(self.output_dir, 'density_features.npy'), self.density_features)
        np.save(os.path.join(self.output_dir, 'layer1_features.npy'), self.layer1_features)
        np.save(os.path.join(self.output_dir, 'layer2_features.npy'), self.layer2_features)

        print(f"✓ All features saved successfully!")

        metadata = {
            'n_samples': self.n_samples,
            'layer1_n_features': self.layer1_features.shape[1],
            'layer2_n_features': self.layer2_features.shape[1],
            'image_size': self.image_size,
            'centroid_type': 'Normalized [0, 1]',
            'note': 'Layer 2 uses normalized centroid coordinates [0, 1]',
        }

        with open(os.path.join(self.output_dir, 'metadata.txt'), 'w') as f:
            f.write("Feature Extraction Metadata\n")
            f.write("=" * 90 + "\n")
            for key, value in metadata.items():
                f.write(f"{key}: {value}\n")

            f.write("\n" + "=" * 90 + "\n")
            f.write("Layer Definition:\n")
            f.write("=" * 90 + "\n")
            f.write("Layer 1 Features (8D):\n")
            f.write("  - Hu moments: 7D (形状特征)\n")
            f.write("  - Area: 1D (大小特征)\n")
            f.write("  - 用途: Stage 1 粗分类\n")
            f.write("\nLayer 2 Features (18D):\n")
            f.write("  - Density: 16D (4x4网格密度)\n")
            f.write("  - Centroid: 2D (归一化坐标 [0-1])\n")
            f.write("  - 用途: Stage 2 细分类（热点检测）\n")
            f.write("\nCentroid Normalization:\n")
            f.write(f"  - Image size: {self.image_size}\n")
            f.write("  - Formula: centroid_norm = centroid_pixel / image_size\n")
            f.write("  - Range: [0, 1]\n")

        print(f"Metadata saved: {os.path.join(self.output_dir, 'metadata.txt')}")

    def visualize_features(self, num_samples=10):
        """Visualize features"""
        print(f"\n[Visualizing {num_samples} samples]")

        fig, axes = plt.subplots(num_samples, 4, figsize=(16, 4 * num_samples))

        image_files = sorted([f for f in os.listdir(self.input_dir) if f.endswith('.png')])[:num_samples]

        for idx, fname in enumerate(image_files):
            image_path = os.path.join(self.input_dir, fname)
            image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)

            _, binary = cv2.threshold(image, 127, 255, cv2.THRESH_BINARY_INV)

            centroid_x_norm = self.centroids_normalized[idx, 0]
            centroid_y_norm = self.centroids_normalized[idx, 1]

            # ══════════════════════════════════════════════════════════
            # Column 1: Original image with centroid
            # ══════════════════════════════════════════════════════════
            ax = axes[idx, 0]
            h, w = image.shape
            ax.imshow(image, cmap='gray')
            # 将归一化坐标转回像素坐标进行显示
            ax.plot(centroid_x_norm * w, centroid_y_norm * h, 'r*', markersize=20)
            ax.axis('off')
            ax.text(0.5, -0.1, f'Centroid: ({centroid_x_norm:.3f}, {centroid_y_norm:.3f})',
                    transform=ax.transAxes, ha='center', fontsize=9,
                    bbox=dict(boxstyle='round', facecolor='yellow', alpha=0.7))
            ax.set_title(fname, fontsize=10, fontweight='bold')

            # ══════════════════════════════════════════════════════════
            # Column 2: Binary image with centroid
            # ══════════════════════════════════════════════════════════
            ax = axes[idx, 1]
            ax.imshow(binary, cmap='binary')
            ax.plot(centroid_x_norm * w, centroid_y_norm * h, 'r*', markersize=20)
            ax.axis('off')
            ax.set_title('Binary Image\n(White=Figure)', fontsize=10, fontweight='bold')

            # ══════════════════════════════════════════════════════════
            # Column 3: Hu moments
            # ══════════════════════════════════════════════════════════
            hu_moms = self.hu_moments[idx]
            axes[idx, 2].bar(range(7), hu_moms, color='steelblue', alpha=0.7)
            axes[idx, 2].set_title(f'Hu Moments\nArea: {self.areas[idx]:.0f}', fontsize=10)
            axes[idx, 2].set_xlabel('Index')
            axes[idx, 2].set_ylabel('Value')
            axes[idx, 2].grid(True, alpha=0.3)

            # ══════════════════════════════════════════════════════════
            # Column 4: Density features
            # ══════════════════════════════════════════════════════════
            density = self.density_features[idx].reshape(4, 4)
            im = axes[idx, 3].imshow(density, cmap='hot', vmin=0, vmax=1)
            axes[idx, 3].set_title('4×4 Grid Density', fontsize=10)
            plt.colorbar(im, ax=axes[idx, 3], fraction=0.046)

        plt.tight_layout()
        plt.show()

    def run(self):
        """Run complete extraction pipeline"""
        print("\n" + "=" * 90)
        print("[Feature Extraction: Hu + Area + Density + Centroid (Normalized)]")
        print("=" * 90)

        layer1_features, layer2_features = self.process_dataset()
        self.compute_statistics()
        self.save_features()
        self.visualize_features(num_samples=5)

        return layer1_features, layer2_features


if __name__ == "__main__":
    try:
        extractor = FeatureExtractorWithCentroidNormalized(output_dir='extracted_features')
        layer1_features, layer2_features = extractor.run()

        print("\n" + "=" * 90)
        print("✓ Feature extraction completed successfully!")
        print("=" * 90)
        print(f"\nLayer 1 Features: {layer1_features.shape} (8D)")
        print(f"  Components: 7 Hu moments + 1 area")
        print(f"\nLayer 2 Features: {layer2_features.shape} (18D)")
        print(f"  Components: 16 grid density + 2 centroid (normalized [0-1]) ⭐")
        print(f"\n✓ Centroid coordinates are normalized to [0, 1] range")

    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()