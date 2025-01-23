// Function to show the content of the selected week
function showContent(weekId) {
  // Hide all content sections
  const sections = document.querySelectorAll('.content-section');
  sections.forEach(section => {
    section.style.display = 'none'; // Hide other sections
  });

  // Show the selected content section
  const selectedSection = document.getElementById(weekId);
  if (selectedSection) {
    selectedSection.style.display = 'flex'; // Display the section
  }
}

// Automatically show the first week's content on page load
document.addEventListener('DOMContentLoaded', () => {
  showContent('week1-2'); // Show week1-2 by default
});
