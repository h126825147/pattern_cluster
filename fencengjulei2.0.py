import numpy as np
import os
import matplotlib.pyplot as plt
from sklearn.cluster import KMeans
from sklearn.metrics import silhouette_score, davies_bouldin_score, calinski_harabasz_score
from sklearn.preprocessing import StandardScaler
from sklearn.decomposition import PCA
import warnings

warnings.filterwarnings('ignore')
plt.rcParams['font.sans-serif'] = ['DejaVu Sans']

print("Current Date and Time (UTC - YYYY-MM-DD HH:MM:SS formatted): 2025-11-04 07:59:08")
print("Current User's Login: JukoYao")
print("-" * 50)


class HierarchicalClustering:
    """双层聚类"""

    def __init__(self, features_dir='extracted_features'):
        self.features_dir = features_dir
        self.layer1_features = None
        self.layer2_features = None
        self.layer1_scaled = None
        self.layer2_scaled = None
        self.n_samples = None

    def load_data(self):
        print("\n[Loading Features]")
        layer1_full = np.load(os.path.join(self.features_dir, 'layer1_features.npy'))
        layer2_full = np.load(os.path.join(self.features_dir, 'layer2_features.npy'))

        self.layer1_features = layer1_full[:, :7]
        self.layer2_features = layer2_full
        self.n_samples = self.layer1_features.shape[0]

        scaler1 = StandardScaler()
        self.layer1_scaled = scaler1.fit_transform(self.layer1_features)

        scaler2 = StandardScaler()
        self.layer2_scaled = scaler2.fit_transform(self.layer2_features)

        print(f"✓ Data prepared: {self.n_samples} samples")

    def apply_weight_layer2(self, features, weight_centroid):
        weights = np.ones(features.shape[1])
        weights[-2:] = weight_centroid
        weights = weights / np.sum(weights)
        return features * weights

    def clustering(self, stage1_k=3, weight_centroid=0.5, min_cluster_size=15):
        print("\n" + "="*100)
        print(f"[Hierarchical Clustering: Stage1 K={stage1_k}, Centroid Weight={weight_centroid}]")
        print("="*100)

        print("\n【Stage 1】Hu Moments Clustering")
        print("-" * 100)
        kmeans_s1 = KMeans(n_clusters=stage1_k, n_init=30, max_iter=300, random_state=42)
        stage1_labels = kmeans_s1.fit_predict(self.layer1_scaled)

        print(f"Stage 1 results (K={stage1_k}):")
        for cid in np.unique(stage1_labels):
            count = np.sum(stage1_labels == cid)
            print(f"  Cluster {cid}: {count} samples")

        print("\n【Stage 2】Density & Centroid Fine-tuning")
        print("-" * 100)
        stage2_labels = np.zeros(self.n_samples, dtype=int)
        current_label = 0
        weighted_l2 = self.apply_weight_layer2(self.layer2_scaled, weight_centroid)

        for cid in np.unique(stage1_labels):
            cluster_indices = np.where(stage1_labels == cid)[0]
            cluster_features = weighted_l2[cluster_indices]
            cluster_size = len(cluster_indices)

            print(f"\nStage1 Cluster {cid}: {cluster_size} samples", end="")

            if cluster_size < min_cluster_size:
                stage2_labels[cluster_indices] = current_label
                print(" → No split (too small)")
                current_label += 1
                continue

            best_k2 = 1
            best_sil = -1
            best_labels = None

            for k2 in [1, 2]:
                if k2 == 1:
                    test_labels = np.zeros(cluster_size, dtype=int)
                    test_sil = -1
                else:
                    kmeans_s2 = KMeans(n_clusters=k2, n_init=10, max_iter=300, random_state=42)
                    test_labels = kmeans_s2.fit_predict(cluster_features)
                    test_sil = silhouette_score(cluster_features, test_labels)

                if test_sil > best_sil:
                    best_sil = test_sil
                    best_k2 = k2
                    best_labels = test_labels

            print(f" → K={best_k2} (Silhouette: {best_sil:.4f})")

            for i, idx in enumerate(cluster_indices):
                stage2_labels[idx] = current_label + best_labels[i]
            current_label += best_k2

        combined_features = np.concatenate([self.layer1_scaled, self.layer2_scaled], axis=1)
        combined_scaled = StandardScaler().fit_transform(combined_features)

        silhouette = silhouette_score(combined_scaled, stage2_labels)
        davies_bouldin = davies_bouldin_score(combined_scaled, stage2_labels)
        calinski = calinski_harabasz_score(combined_scaled, stage2_labels)
        n_clusters = len(np.unique(stage2_labels))

        print(f"\n✓ Final Results: {n_clusters} clusters")
        for cid in np.unique(stage2_labels):
            count = np.sum(stage2_labels == cid)
            print(f"  Cluster {cid}: {count} samples")

        print(f"\n✓ Metrics:")
        print(f"  Silhouette Score:    {silhouette:.4f}")
        print(f"  Davies-Bouldin Index: {davies_bouldin:.4f}")
        print(f"  Calinski-Harabasz:   {calinski:.1f}")

        return stage2_labels, silhouette, davies_bouldin, calinski, n_clusters

    def visualize(self, labels, n_clusters, silhouette, davies_bouldin, calinski):
        print("\n[Generating Visualizations]")

        combined_features = np.concatenate([self.layer1_scaled, self.layer2_scaled], axis=1)
        pca = PCA(n_components=2)
        features_2d = pca.fit_transform(combined_features)

        fig = plt.figure(figsize=(16, 10))
        gs = fig.add_gridspec(2, 2, hspace=0.3, wspace=0.3)

        # Clustering result
        ax1 = fig.add_subplot(gs[0, :])
        unique_clusters = np.unique(labels)
        colors = plt.cm.tab10(np.linspace(0, 1, len(unique_clusters)))

        for cid, color in zip(unique_clusters, colors):
            mask = labels == cid
            ax1.scatter(
                features_2d[mask, 0],
                features_2d[mask, 1],
                c=[color],
                label=f'Cluster {cid} ({np.sum(mask)} samples)',
                s=120,
                alpha=0.7,
                edgecolors='black',
                linewidth=1.5
            )

        ax1.set_xlabel(f'PC1 ({pca.explained_variance_ratio_[0]:.1%})', fontsize=12, fontweight='bold')
        ax1.set_ylabel(f'PC2 ({pca.explained_variance_ratio_[1]:.1%})', fontsize=12, fontweight='bold')
        ax1.set_title(f'Hierarchical Clustering Results (K={n_clusters})', fontsize=14, fontweight='bold')
        ax1.grid(True, alpha=0.3)
        ax1.legend(fontsize=10, loc='best', ncol=min(3, len(unique_clusters)))

        # Metrics
        metrics_names = ['Silhouette\nScore', 'Davies-Bouldin\nIndex', 'Calinski-Harabasz\nIndex']
        metrics_values = [silhouette, davies_bouldin, calinski]
        colors_metric = ['#51CF66', '#FF6B6B', '#4ECDC4']

        ax2 = fig.add_subplot(gs[1, 0])
        bars = ax2.bar(metrics_names, metrics_values, color=colors_metric, alpha=0.7, edgecolor='black', linewidth=2)
        ax2.set_ylabel('Value', fontsize=11, fontweight='bold')
        ax2.set_title('Evaluation Metrics', fontsize=12, fontweight='bold')
        ax2.grid(True, alpha=0.3, axis='y')
        for bar, val in zip(bars, metrics_values):
            ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height(),
                    f'{val:.2f}', ha='center', va='bottom', fontsize=10, fontweight='bold')

        # Cluster distribution
        ax3 = fig.add_subplot(gs[1, 1])
        cluster_counts = [np.sum(labels == cid) for cid in unique_clusters]
        bars = ax3.bar(unique_clusters, cluster_counts, color=colors, alpha=0.7, edgecolor='black', linewidth=2)
        ax3.set_xlabel('Cluster ID', fontsize=11, fontweight='bold')
        ax3.set_ylabel('Sample Count', fontsize=11, fontweight='bold')
        ax3.set_title('Cluster Distribution', fontsize=12, fontweight='bold')
        ax3.grid(True, alpha=0.3, axis='y')
        for bar, count in zip(bars, cluster_counts):
            ax3.text(bar.get_x() + bar.get_width()/2, bar.get_height(),
                    f'{int(count)}', ha='center', va='bottom', fontsize=10, fontweight='bold')

        plt.suptitle('Hierarchical Clustering Analysis', fontsize=15, fontweight='bold')
        plt.show()

    def run(self, stage1_k=3, weight_centroid=0.5, min_cluster_size=15):
        print("\n" + "="*100)
        print("[Hierarchical Clustering]")
        print("="*100)

        self.load_data()
        labels, silhouette, davies_bouldin, calinski, n_clusters = self.clustering(
            stage1_k=stage1_k,
            weight_centroid=weight_centroid,
            min_cluster_size=min_cluster_size
        )
        self.visualize(labels, n_clusters, silhouette, davies_bouldin, calinski)

        print("\n" + "="*100)
        print("✓ Clustering Complete!")
        print("="*100)

        return labels, silhouette, davies_bouldin, calinski, n_clusters


if __name__ == "__main__":
    clustering = HierarchicalClustering(features_dir='extracted_features')
    labels, sil, db, calinski, k = clustering.run(
        stage1_k=3,
        weight_centroid=0.5,
        min_cluster_size=15
    )