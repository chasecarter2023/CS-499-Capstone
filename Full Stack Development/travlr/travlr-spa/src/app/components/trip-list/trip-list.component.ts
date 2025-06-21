// src/app/components/trip-list/trip-list.component.ts
import { Component, OnInit } from '@angular/core';
import { CommonModule } from '@angular/common';
import { TripService, Trip } from '../../services/trip.service';
import { TripCardComponent } from '../trip-card/trip-card.component';
import { RouterModule } from '@angular/router';

@Component({
  selector: 'app-trip-list',
  standalone: true,
  imports: [CommonModule, TripCardComponent, RouterModule],
  templateUrl: './trip-list.component.html',
  styleUrls: ['./trip-list.component.css']
})
export class TripListComponent implements OnInit {
  trips: Trip[] = [];

  constructor(private tripService: TripService) {}

  ngOnInit(): void {
    this.loadTrips();
  }

  // Load all trips from the service
  loadTrips(): void {
    this.tripService.getTrips().subscribe(data => {
      this.trips = data;
    });
  }

  // Handler for deleting a trip
  handleDelete(tripId: string): void {
    // Optional: Add a confirmation dialog
    if (confirm('Are you sure you want to delete this trip?')) {
      this.tripService.deleteTrip(tripId).subscribe(() => {
        // Update the trips array to remove the deleted trip
        this.trips = this.trips.filter(trip => trip.id !== tripId);
      });
    }
  }
}
